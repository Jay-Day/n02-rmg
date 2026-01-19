#pragma once

#include <cstring>
#include <algorithm>

// P2P instruction types
#define LOGN    0
#define PING    1
#define PREADY  2
#define TSYNC   3
#define TTIME   4
#define LOAD    5
#define START   6
#define DATA    7
#define DROP    8
#define CHAT    9
#define EXIT    10

// LOGN flags
#define LOGN_REQ   0
#define LOGN_RPOS  1
#define LOGN_RNEG  2
#define LOGN_CFM   3

// PING flags
#define PING_PING  0
#define PING_ECHO  1

// PREADY flags
#define PREADY_READY   0
#define PREADY_NREADY  1

// LOAD flags
#define LOAD_LOAD       0
#define LOAD_LOADED     1
#define LOAD_LOADEDACK  2

// TTIME flags
#define TTIME_TTIME  0

// START flags
#define START_START  0

// DROP flags
#define DROP_DROP  0

// TSYNC flags
#define TSYNC_FORCE   1
#define TSYNC_CHECK   2
#define TSYNC_ADJUST  4

// CHAT flags
#define CHAT_32   0
#define CHAT_64   1
#define CHAT_128  2

#pragma pack(push, 1)

struct p2p_instruction_st {
    unsigned type : 4;
    unsigned flags : 4;
    unsigned char body[256];
};

#pragma pack(pop)

class p2p_instruction {
public:
    p2p_instruction_st inst;
    unsigned char pos;
    unsigned char len;

    p2p_instruction();
    p2p_instruction(int type, int flags);

    void clone(p2p_instruction* other);
    unsigned char size();

    // Store methods
    void store_bytes(const void* data, int datalen);
    void store_string(const char* str);      // 128 bytes
    void store_sstring(const char* str);     // 32 bytes
    void store_mstring(const char* str);     // 64 bytes
    void store_vstring(const char* str);     // variable length

    void store_int(int x);
    void store_uint(unsigned int x);
    void store_short(short x);
    void store_ushort(unsigned short x);
    void store_char(char x);
    void store_uchar(unsigned char x);

    // Load methods
    void load_bytes(void* data, int datalen);
    void load_string(char* str);
    void load_sstring(char* str);
    void load_mstring(char* str);
    void load_vstring(char* str);

    int load_int();
    unsigned int load_uint();
    short load_short();
    unsigned short load_ushort();
    char load_char();
    unsigned char load_uchar();

    // Serialization
    int write_to_message(char* buffer);
    void read_from_message(char* buffer, int buflen);

    // Debug
    void to_string();
    void to_string(char* buf);
};
