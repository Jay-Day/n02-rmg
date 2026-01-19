#pragma once

#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#include "containers.h"

class k_socket {
protected:
public:
    unsigned short port;
    int sock;
    bool has_data_waiting;
    sockaddr_in addr;

    static slist<k_socket*, FD_SETSIZE> list;
    static int ndfs;
    static fd_set sockets;

public:
    k_socket();
    virtual ~k_socket();

    virtual int clone(k_socket* remote);
    void close();

    virtual bool initialize(int param_port, int minbuffersize = 32 * 1024);
    virtual bool set_address(const char* cp, unsigned short hostshort);
    virtual bool set_addr(sockaddr_in* arg_addr);
    virtual bool set_aport(int port);
    virtual bool send(const char* buf, int len);
    virtual bool check_recv(char* buf, int* len, bool leave_in_queue, sockaddr_in* addrp);
    virtual bool has_data();
    virtual int get_port();
    char* to_string(char* buf);

    static bool check_sockets(int secs, int ms);
    static bool Initialize();
    static void Cleanup();
};
