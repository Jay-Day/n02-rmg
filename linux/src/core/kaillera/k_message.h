#pragma once

#include "common/k_socket.h"
#include "common/containers.h"
#include "common/stats.h"
#include "k_instruction.h"

#define K_ICACHESIZE 16

#pragma pack(push, 1)

struct k_instruction_head {
    unsigned short serial;
    unsigned short length;
};

#pragma pack(pop)

struct k_instruction_ptr {
    k_instruction_head head;
    char* body;
};

class k_message : public k_socket {
public:
    unsigned short last_sent_instruction;
    unsigned short last_processed_instruction;
    unsigned short last_cached_instruction;
    oslist<k_instruction_ptr, K_ICACHESIZE> out_cache;
    oslist<k_instruction_ptr, K_ICACHESIZE> in_cache;
    int default_ipm;

    k_message();
    ~k_message();

    void send_instruction(k_instruction* inst);
    bool send(const char* buf, int len);
    bool send_message(int limit);
    bool receive_instruction(k_instruction* inst, bool leave_in_queue, sockaddr_in* addr);
    bool check_recv(char* buf, int* len, bool leave_in_queue, sockaddr_in* addrp);
    bool has_data();
    void resend_message(int limit);
};
