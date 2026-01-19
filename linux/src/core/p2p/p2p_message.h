#pragma once

#include "common/k_socket.h"
#include "common/containers.h"
#include "common/stats.h"
#include "p2p_instruction.h"

#define ICACHESIZE 32

#pragma pack(push, 1)

struct p2p_instruction_head {
    unsigned char serial;
    unsigned char length;
};

#pragma pack(pop)

struct p2p_instruction_ptr {
    p2p_instruction_head head;
    char body[256];
};

class p2p_message : public k_socket {
    unsigned char last_sent_instruction;
    unsigned char last_processed_instruction;
    unsigned char last_cached_instruction;
    oslist<p2p_instruction_ptr, ICACHESIZE> out_cache;
    oslist<p2p_instruction_ptr, ICACHESIZE> in_cache;

public:
    odlist<char*, 1> ssrv_cache;
    int default_ipm;
    int dsc;

    p2p_message();
    ~p2p_message();

    void send_instruction(p2p_instruction* inst);
    void send_tinst(int type, int flags);
    void send_ssrv(char* buf, int buflen, sockaddr_in* addr);
    void send_ssrv(char* buf, int buflen, const char* host, int port);

    bool has_ssrv();
    int receive_ssrv(char* buf, sockaddr_in* addr);

    bool send(const char* buf, int len);
    bool send_message(int limit);

    bool receive_instruction(p2p_instruction* inst, bool leave_in_queue, sockaddr_in* addr);
    bool receive_instructionx(p2p_instruction* inst);

    bool check_recvx(char* buf, int* len);
    bool check_recv(char* buf, int* len, bool leave_in_queue, sockaddr_in* addrp);

    bool has_data();
    void resend_message(int limit);
};
