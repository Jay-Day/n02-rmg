#include "p2p_message.h"
#include <cstring>
#include <algorithm>
#include <cstdlib>

p2p_message::p2p_message() : k_socket() {
    last_sent_instruction = 0;
    last_processed_instruction = 0;
    last_cached_instruction = static_cast<unsigned char>(-1);
    default_ipm = 3;
    dsc = 0;
}

p2p_message::~p2p_message() {
    // Clean up ssrv_cache
    for (int i = 0; i < ssrv_cache.length; i++) {
        std::free(ssrv_cache[i]);
    }
}

void p2p_message::send_instruction(p2p_instruction* inst) {
    p2p_message::send((const char*)inst, inst->size());
}

void p2p_message::send_tinst(int type, int flags) {
    p2p_instruction kx(type, flags);
    p2p_message::send((const char*)&kx, kx.size());
}

void p2p_message::send_ssrv(char* bf, int bflen, sockaddr_in* arg_addr) {
    sockaddr_in sad = addr;
    set_addr(arg_addr);
    char bufferr[1000];
    bufferr[0] = 0;
    std::memcpy(&bufferr[1], bf, bflen);
    k_socket::send(bufferr, bflen + 1);
    set_addr(&sad);
}

void p2p_message::send_ssrv(char* bf, int bflen, const char* host, int port) {
    sockaddr_in sad = addr;
    set_address(host, port);
    char bufferr[1000];
    bufferr[0] = 0;
    std::memcpy(&bufferr[1], bf, bflen);
    k_socket::send(bufferr, bflen + 1);
    set_addr(&sad);
}

bool p2p_message::has_ssrv() {
    return ssrv_cache.length > 0;
}

int p2p_message::receive_ssrv(char* buf, sockaddr_in* addrr) {
    if (ssrv_cache.length > 0) {
        char* bbb = ssrv_cache[0];
        int len = *((short*)bbb);
        std::memcpy(buf, bbb + 2 + sizeof(sockaddr_in), len);
        std::memcpy(addrr, bbb + 2, sizeof(sockaddr_in));
        std::free(bbb);
        ssrv_cache.removei(0);
        return len;
    }
    return 0;
}

bool p2p_message::send(const char* buf, int len) {
    unsigned char vx = last_sent_instruction++;
    p2p_instruction* ibuf = (p2p_instruction*)buf;

    if (out_cache.size() == ICACHESIZE - 1) {
        out_cache.removei(0);
    }

    p2p_instruction_ptr kip;
    kip.head.serial = vx;
    kip.head.length = ibuf->write_to_message(kip.body);
    out_cache.add(kip);

    send_message(default_ipm);
    return true;
}

bool p2p_message::send_message(int limit) {
    char buf[0x1000];
    int len = 1;
    char* buff = buf;
    int max_t = std::min(out_cache.size(), limit);
    *buff++ = (char)max_t;

    if (max_t > 0) {
        for (int i = 0; i < max_t; i++) {
            int cache_index = out_cache.size() - i - 1;
            *(p2p_instruction_head*)buff = out_cache[cache_index].head;
            buff += sizeof(p2p_instruction_head);
            int l = out_cache[cache_index].head.length;
            std::memcpy(buff, out_cache[cache_index].body, l);
            buff += l;
            len += l + sizeof(p2p_instruction_head);
        }
    }

    if (dsc > 0) {
        dsc--;
    } else {
        k_socket::send(buf, len);
    }
    return true;
}

bool p2p_message::receive_instruction(p2p_instruction* arg_0, bool leave_in_queue, sockaddr_in* arg_8) {
    char var_8000[1024];
    int var_8004 = 1024;
    if (check_recv(var_8000, &var_8004, leave_in_queue, arg_8)) {
        arg_0->read_from_message(var_8000, var_8004);
        return true;
    }
    return false;
}

bool p2p_message::receive_instructionx(p2p_instruction* arg_0) {
    char var_8000[1024];
    int var_8004 = 1024;
    if (check_recvx(var_8000, &var_8004)) {
        arg_0->read_from_message(var_8000, var_8004);
        return true;
    }
    return false;
}

bool p2p_message::check_recvx(char* buf, int* len) {
    if (has_data_waiting) {
        sockaddr_in addrp;
        char buff[2024];
        int bufflen = 2024;
        if (k_socket::check_recv(buff, &bufflen, false, &addrp) &&
            (addrp.sin_addr.s_addr == addr.sin_addr.s_addr)) {
            unsigned char instruction_count = (unsigned char)*buff;
            char* ptr = buff + 1;
            if (instruction_count > 0 && instruction_count < 15) {
                unsigned char latest_serial = (unsigned char)*ptr;
                int si = in_cache.size();
                unsigned char tx = latest_serial - last_cached_instruction;
                if (tx > 0 && tx < 15) {
                    in_cache.set_size(si + tx);
                    for (int u = 0; u < instruction_count; u++) {
                        unsigned char serial = ((p2p_instruction_head*)ptr)->serial;
                        unsigned char length = ((p2p_instruction_head*)ptr)->length;
                        ptr += sizeof(p2p_instruction_head);
                        if (serial == last_cached_instruction)
                            break;
                        unsigned char cix = serial - last_cached_instruction;
                        PACKETLOSSCOUNT += cix - 1;
                        int ind = si + (cix) - 1;
                        in_cache.items[ind].head.serial = serial;
                        in_cache.items[ind].head.length = length;
                        std::memcpy(in_cache.items[ind].body, ptr, length);
                        ptr += length;
                    }
                    last_cached_instruction = latest_serial;
                } else if (tx == 0) {
                    SOCK_RECV_RETR++;
                } else {
                    PACKETMISOTDERCOUNT++;
                }
            }
        }
    }

    if (in_cache.size() > 0) {
        *len = in_cache[0].head.length;
        std::memcpy(buf, in_cache[0].body, *len);
        last_processed_instruction = in_cache[0].head.serial;
        in_cache.removei(0);
        return true;
    }
    return false;
}

bool p2p_message::check_recv(char* buf, int* len, bool leave_in_queue, sockaddr_in* addrp) {
    if (has_data_waiting) {
        char buff[2024];
        int bufflen = 2024;
        if (k_socket::check_recv(buff, &bufflen, false, addrp)) {
            unsigned char instruction_count = (unsigned char)*buff;
            char* ptr = buff + 1;
            if (instruction_count > 0 && instruction_count < 15) {
                unsigned char latest_serial = (unsigned char)*ptr;
                int si = in_cache.size();
                unsigned char tx = latest_serial - last_cached_instruction;
                if (tx > 0 && tx < 15) {
                    in_cache.set_size(si + tx);
                    for (int u = 0; u < instruction_count; u++) {
                        unsigned char serial = ((p2p_instruction_head*)ptr)->serial;
                        unsigned char length = ((p2p_instruction_head*)ptr)->length;
                        ptr += sizeof(p2p_instruction_head);
                        if (serial == last_cached_instruction)
                            break;
                        unsigned char cix = serial - last_cached_instruction;
                        PACKETLOSSCOUNT += cix - 1;
                        int ind = si + (cix) - 1;
                        in_cache.items[ind].head.serial = serial;
                        in_cache.items[ind].head.length = length;
                        std::memcpy(in_cache.items[ind].body, ptr, length);
                        ptr += length;
                    }
                    last_cached_instruction = latest_serial;
                } else if (tx == 0) {
                    SOCK_RECV_RETR++;
                } else {
                    PACKETMISOTDERCOUNT++;
                }
            } else {
                // Server service message (instruction_count == 0)
                if (instruction_count == 0 && bufflen > 5) {
                    char* inb = (char*)std::malloc(bufflen + 1 + sizeof(sockaddr_in));
                    *((short*)inb) = bufflen - 1;
                    *((sockaddr_in*)(inb + 2)) = *addrp;
                    std::memcpy(inb + 2 + sizeof(sockaddr_in), buff + 1, bufflen);
                    ssrv_cache.add(inb);
                }
            }
        }
    }

    if (in_cache.size() > 0) {
        *len = in_cache[0].head.length;
        std::memcpy(buf, in_cache[0].body, *len);
        if (!leave_in_queue) {
            last_processed_instruction = in_cache[0].head.serial;
            in_cache.removei(0);
        }
        return true;
    }
    return false;
}

bool p2p_message::has_data() {
    if (in_cache.length == 0)
        return has_data_waiting;
    else
        return true;
}

void p2p_message::resend_message(int limit) {
    send_message(limit);
}
