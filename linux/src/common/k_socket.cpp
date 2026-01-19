#include "k_socket.h"
#include "stats.h"
#include <cstdio>

// Initialize static members
slist<k_socket*, FD_SETSIZE> k_socket::list;
int k_socket::ndfs = 0;
fd_set k_socket::sockets;

// Initialize stats (defined here, can be moved to stats.cpp later)
int PACKETLOSSCOUNT = 0;
int PACKETMISOTDERCOUNT = 0;
int PACKETINCDSCCOUNT = 0;
int PACKETIADSCCOUNT = 0;
int SOCK_RECV_PACKETS = 0;
int SOCK_RECV_BYTES = 0;
int SOCK_RECV_RETR = 0;
int SOCK_SEND_PACKETS = 0;
int SOCK_SEND_BYTES = 0;
int SOCK_SEND_RETR = 0;

k_socket::k_socket() {
    sock = -1;
    list.add(this);
    has_data_waiting = false;
    if (ndfs == 0) {
        FD_ZERO(&sockets);
    }
    port = 0;
}

k_socket::~k_socket() {
    close();
}

int k_socket::clone(k_socket* remote) {
    port = remote->port;
    sock = remote->sock;
    return 0;
}

void k_socket::close() {
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        ::close(sock);
        FD_CLR(sock, &sockets);
        if (sock == ndfs) {
            ndfs = 0;
            for (int i = 0; i < list.size(); i++) {
                k_socket* ks = list[i];
                if (ks != this && ks->sock > ndfs) {
                    ndfs = ks->sock;
                }
            }
        }
        sock = -1;
    }
    list.remove(this);
}

bool k_socket::initialize(int param_port, int minbuffersize) {
    port = param_port;

    sockaddr_in tempaddr;
    std::memset(&tempaddr, 0, sizeof(tempaddr));
    tempaddr.sin_family = AF_INET;
    tempaddr.sin_port = htons(param_port);

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        return false;
    }

    tempaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (sockaddr*)&tempaddr, sizeof(tempaddr)) != 0) {
        ::close(sock);
        sock = -1;
        return false;
    }

    // Set non-blocking mode
    unsigned long ul = 1;
    if (ioctl(sock, FIONBIO, &ul) < 0) {
        ::close(sock);
        sock = -1;
        return false;
    }

    FD_SET(sock, &sockets);
    if (sock > ndfs) {
        ndfs = sock;
    }

    // Get actual port if 0 was requested (random port)
    if (port == 0) {
        socklen_t addrlen = sizeof(tempaddr);
        getsockname(sock, (sockaddr*)&tempaddr, &addrlen);
        port = ntohs(tempaddr.sin_port);
    }

    // Set receive buffer size if needed
    if (minbuffersize > 0) {
        int val = 0;
        socklen_t optlen = sizeof(val);
        getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &val, &optlen);
        if (val < minbuffersize) {
            val = minbuffersize;
            setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
        }
    }

    return true;
}

bool k_socket::set_address(const char* cp, unsigned short hostshort) {
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(hostshort);

    if (addr.sin_port == 0) {
        return false;
    }

    // Try as IP address first
    if (inet_pton(AF_INET, cp, &addr.sin_addr) == 1) {
        return true;
    }

    // Try DNS lookup using getaddrinfo (modern replacement for gethostbyname)
    struct addrinfo hints, *result;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(cp, nullptr, &hints, &result) == 0) {
        if (result != nullptr) {
            sockaddr_in* addr_in = (sockaddr_in*)result->ai_addr;
            addr.sin_addr = addr_in->sin_addr;
            freeaddrinfo(result);
            return true;
        }
        freeaddrinfo(result);
    }

    return false;
}

bool k_socket::set_addr(sockaddr_in* arg_addr) {
    std::memcpy(&addr, arg_addr, sizeof(addr));
    return true;
}

bool k_socket::set_aport(int port) {
    addr.sin_port = htons(port);
    return addr.sin_port != 0;
}

bool k_socket::send(const char* buf, int len) {
    SOCK_SEND_PACKETS++;
    SOCK_SEND_BYTES += len;
    ssize_t result = sendto(sock, buf, len, 0, (sockaddr*)&addr, sizeof(addr));
    return result >= 0;
}

bool k_socket::check_recv(char* buf, int* len, bool leave_in_queue, sockaddr_in* addrp) {
    sockaddr_in saa;
    socklen_t addrlen = sizeof(saa);
    has_data_waiting = false;

    ssize_t received = recvfrom(sock, buf, *len, leave_in_queue ? MSG_PEEK : 0,
                                 (sockaddr*)&saa, &addrlen);
    if (received > 0) {
        *len = (int)received;
        std::memcpy(addrp, &saa, sizeof(saa));
        SOCK_RECV_PACKETS++;
        SOCK_RECV_BYTES += received;
        return true;
    }

    return false;
}

bool k_socket::has_data() {
    return has_data_waiting;
}

int k_socket::get_port() {
    return port;
}

char* k_socket::to_string(char* buf) {
    std::sprintf(buf, "k_socket {\n\tsock: %d;\n\tport: %u;\n\thas_data: %d;\n};",
                 sock, port, has_data_waiting);
    return buf;
}

bool k_socket::check_sockets(int secs, int ms) {
    timeval tv;
    tv.tv_sec = secs;
    tv.tv_usec = ms * 1000;

    fd_set temp;
    std::memcpy(&temp, &sockets, sizeof(fd_set));

    if (select(ndfs + 1, &temp, nullptr, nullptr, &tv) > 0) {
        for (int i = 0; i < list.size(); i++) {
            k_socket* k = list[i];
            if (FD_ISSET(k->sock, &temp)) {
                k->has_data_waiting = true;
            }
        }
        return true;
    }
    return false;
}

bool k_socket::Initialize() {
    // No-op on Linux (no WSAStartup needed)
    return true;
}

void k_socket::Cleanup() {
    // No-op on Linux (no WSACleanup needed)
}
