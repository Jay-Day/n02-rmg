#include "p2p_instruction.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

static const char* ITYPES[] = {
    "LOGN", "PING", "PREADY", "TSYNC", "TTIME",
    "LOAD", "START", "DATA", "DROP", "CHAT", "EXIT"
};

p2p_instruction::p2p_instruction() : pos(0), len(0) {
    std::memset(&inst, 0, sizeof(inst));
}

p2p_instruction::p2p_instruction(int type, int flags) : pos(0), len(0) {
    inst.type = type;
    inst.flags = flags;
}

void p2p_instruction::clone(p2p_instruction* other) {
    inst = other->inst;
    pos = other->pos;
    len = other->len;
}

unsigned char p2p_instruction::size() {
    return std::min(255, (int)(len + 1));
}

void p2p_instruction::store_bytes(const void* data, int datalen) {
    int fpos = std::min(datalen + (int)pos, 255);
    int le = fpos - pos;
    std::memcpy(&inst.body[pos], data, le);
    pos += le;
}

void p2p_instruction::store_string(const char* str) {
    store_bytes(str, 128);
}

void p2p_instruction::store_sstring(const char* str) {
    store_bytes(str, 32);
}

void p2p_instruction::store_mstring(const char* str) {
    store_bytes(str, 64);
}

void p2p_instruction::store_vstring(const char* str) {
    store_bytes(str, std::min((int)std::strlen(str) + 1, 251));
}

void p2p_instruction::store_int(int x) {
    store_bytes(&x, 4);
}

void p2p_instruction::store_uint(unsigned int x) {
    store_bytes(&x, 4);
}

void p2p_instruction::store_short(short x) {
    store_bytes(&x, 2);
}

void p2p_instruction::store_ushort(unsigned short x) {
    store_bytes(&x, 2);
}

void p2p_instruction::store_char(char x) {
    store_bytes(&x, 1);
}

void p2p_instruction::store_uchar(unsigned char x) {
    store_bytes(&x, 1);
}

void p2p_instruction::load_bytes(void* data, int datalen) {
    if (pos + datalen <= len) {
        std::memcpy(data, &inst.body[pos], datalen);
        pos += datalen;
    }
}

void p2p_instruction::load_string(char* str) {
    load_bytes(str, 128);
    str[127] = '\0';
}

void p2p_instruction::load_sstring(char* str) {
    load_bytes(str, 32);
    str[31] = '\0';
}

void p2p_instruction::load_mstring(char* str) {
    load_bytes(str, 64);
    str[63] = '\0';
}

void p2p_instruction::load_vstring(char* str) {
    unsigned char sl = std::min((unsigned char)(std::strlen((char*)&inst.body[pos]) + 1),
                                 (unsigned char)(256 - pos));
    load_bytes(str, sl);
    str[sl - 1] = '\0';
}

int p2p_instruction::load_int() {
    int x = 0;
    load_bytes(&x, 4);
    return x;
}

unsigned int p2p_instruction::load_uint() {
    unsigned int x = 0;
    load_bytes(&x, 4);
    return x;
}

short p2p_instruction::load_short() {
    short x = 0;
    load_bytes(&x, 2);
    return x;
}

unsigned short p2p_instruction::load_ushort() {
    unsigned short x = 0;
    load_bytes(&x, 2);
    return x;
}

char p2p_instruction::load_char() {
    char x = 0;
    load_bytes(&x, 1);
    return x;
}

unsigned char p2p_instruction::load_uchar() {
    unsigned char x = 0;
    load_bytes(&x, 1);
    return x;
}

int p2p_instruction::write_to_message(char* buffer) {
    *buffer++ = *((char*)&inst);
    std::memcpy(buffer, inst.body, pos);
    return 1 + pos;
}

void p2p_instruction::read_from_message(char* buffer, int buflen) {
    *((char*)&inst) = *buffer++;
    len = buflen - 1;
    std::memcpy(inst.body, buffer, len);
    pos = 0;
}

void p2p_instruction::to_string() {
    // Debug output (could use logging framework)
}

void p2p_instruction::to_string(char* buf) {
    std::sprintf(buf, "p2p_instruction {%s, %d}", ITYPES[inst.type], inst.flags);
}
