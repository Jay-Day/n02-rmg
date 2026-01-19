#include "k_message.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>

k_message::k_message() : k_socket() {
    last_sent_instruction = 0;
    last_processed_instruction = 0;
    last_cached_instruction = static_cast<unsigned short>(-1);
    default_ipm = 3;
}

k_message::~k_message() {
    // Clean up cached instruction bodies
    for (int i = 0; i < out_cache.length; i++) {
        std::free(out_cache.items[i].body);
    }
    for (int i = 0; i < in_cache.length; i++) {
        std::free(in_cache.items[i].body);
    }
}

void k_message::send_instruction(k_instruction* inst) {
    char temp[32768];
    int l = inst->write_to_message(temp, 32767);
    k_message::send(temp, l);
}

bool k_message::send(const char* buf, int len) {
    unsigned short vx = last_sent_instruction++;

    if (out_cache.size() == K_ICACHESIZE - 1) {
        std::free(out_cache[0].body);
        out_cache.removei(0);
    }

    k_instruction_ptr kip;
    kip.body = (char*)std::malloc(len);
    kip.head.serial = vx;
    kip.head.length = len;
    std::memcpy(kip.body, buf, len);
    out_cache.add(kip);

    send_message(default_ipm);
    return true;
}

bool k_message::send_message(int limit) {
    char buf[0x1000];
    int len = 1;
    char* buff = buf;
    int max_t = std::min(out_cache.size(), limit);
    *buff++ = (char)max_t;

    if (max_t > 0) {
        for (int i = 0; i < max_t; i++) {
            int cache_index = out_cache.size() - i - 1;
            *(k_instruction_head*)buff = out_cache[cache_index].head;
            buff += sizeof(k_instruction_head);
            int l = out_cache[cache_index].head.length;
            std::memcpy(buff, out_cache[cache_index].body, l);
            buff += l;
            len += l + sizeof(k_instruction_head);
        }
    }

    k_socket::send(buf, len);
    return true;
}

bool k_message::receive_instruction(k_instruction* inst, bool leave_in_queue, sockaddr_in* addr) {
    char var_8000[0x4E20];
    int var_8004 = 0x4E20;
    if (check_recv(var_8000, &var_8004, leave_in_queue, addr)) {
        inst->read_from_message(var_8000, var_8004);
        return true;
    }
    return false;
}

bool k_message::check_recv(char* buf, int* len, bool leave_in_queue, sockaddr_in* addrp) {
    if (has_data_waiting) {
        char buff[0x5E20];
        int bufflen = 0x5E20;
        if (k_socket::check_recv(buff, &bufflen, false, addrp)) {
            unsigned char instruction_count = (unsigned char)*buff;
            char* ptr = buff + 1;

            if (instruction_count != 0) {
                unsigned short latest_serial = *((unsigned short*)(ptr));
                int si = in_cache.size();

                // On first packet, initialize last_cached_instruction
                if (last_cached_instruction == static_cast<unsigned short>(-1)) {
                    last_cached_instruction = latest_serial - 1;
                }

                unsigned short tx = latest_serial - last_cached_instruction;

                if (tx > 0 && tx < 10) {
                    in_cache.set_size(si + tx);

                    for (int u = 0; u < instruction_count; u++) {
                        unsigned short serial = ((k_instruction_head*)ptr)->serial;
                        unsigned short length = ((k_instruction_head*)ptr)->length;
                        ptr += sizeof(k_instruction_head);

                        if (serial == last_cached_instruction) {
                            break;
                        }

                        unsigned short cix = serial - last_cached_instruction;
                        PACKETLOSSCOUNT += cix - 1;

                        int ind = si + (cix) - 1;

                        in_cache.items[ind].head.serial = serial;
                        in_cache.items[ind].head.length = length;
                        in_cache.items[ind].body = (char*)std::malloc(length);
                        std::memcpy(in_cache.items[ind].body, ptr, length);

                        ptr += length;
                    }

                    last_cached_instruction = latest_serial;
                }
            }
        }
    }

    if (in_cache.size() > 0) {
        *len = in_cache[0].head.length;
        std::memcpy(buf, in_cache[0].body, *len);

        if (!leave_in_queue) {
            last_processed_instruction = in_cache[0].head.serial;
            std::free(in_cache[0].body);
            in_cache.removei(0);
        }

        return true;
    }
    return false;
}

bool k_message::has_data() {
    if (in_cache.length == 0) {
        return has_data_waiting;
    }
    return true;
}

void k_message::resend_message(int limit) {
    SOCK_SEND_RETR++;
    send_message(limit);
}
