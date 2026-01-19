#pragma once

#include <cstring>
#include <cstdlib>
#include <algorithm>

// Kaillera instruction types
enum INSTRUCTION {
    INVDNONE, USERLEAV, USERJOIN, USERLOGN, LONGSUCC, SERVPING, USERPONG, PARTCHAT,
    GAMECHAT, TMOUTRST, GAMEMAKE, GAMRLEAV, GAMRJOIN, GAMRSLST, GAMESTAT, GAMRKICK,
    GAMESHUT, GAMEBEGN, GAMEDATA, GAMCDATA, GAMRDROP, GAMRSRDY, LOGNSTAT, MOTDLINE
};

#define INSTRUCTION_CLNTSKIP INVDNONE
#define INSTRUCTION_USERLEAV USERLEAV
#define INSTRUCTION_USERJOIN USERJOIN
#define INSTRUCTION_USERLOGN USERLOGN
#define INSTRUCTION_LONGSUCC LONGSUCC
#define INSTRUCTION_SERVPING SERVPING
#define INSTRUCTION_USERPONG USERPONG
#define INSTRUCTION_PARTCHAT PARTCHAT
#define INSTRUCTION_GAMECHAT GAMECHAT
#define INSTRUCTION_TMOUTRST TMOUTRST
#define INSTRUCTION_GAMEMAKE GAMEMAKE
#define INSTRUCTION_GAMRLEAV GAMRLEAV
#define INSTRUCTION_GAMRJOIN GAMRJOIN
#define INSTRUCTION_GAMRSLST GAMRSLST
#define INSTRUCTION_GAMESTAT GAMESTAT
#define INSTRUCTION_GAMRKICK GAMRKICK
#define INSTRUCTION_GAMESHUT GAMESHUT
#define INSTRUCTION_GAMEBEGN GAMEBEGN
#define INSTRUCTION_GAMEDATA GAMEDATA
#define INSTRUCTION_GAMCDATA GAMCDATA
#define INSTRUCTION_GAMRDROP GAMRDROP
#define INSTRUCTION_GAMRSRDY GAMRSRDY
#define INSTRUCTION_LOGNSTAT LOGNSTAT
#define INSTRUCTION_MOTDCHAT MOTDLINE

class k_instruction {
public:
    INSTRUCTION type;
    char user[32];
    char* buffer;
    unsigned int buffer_len;
    unsigned int buffer_pos;

    k_instruction() : type(INVDNONE), buffer_pos(0), buffer_len(16) {
        user[0] = '\0';
        buffer = (char*)std::malloc(buffer_len);
        *buffer = 0x00;
    }

    ~k_instruction() {
        std::free(buffer);
    }

    // Non-copyable (has raw pointer)
    k_instruction(const k_instruction&) = delete;
    k_instruction& operator=(const k_instruction&) = delete;

    void clone(k_instruction* other) {
        type = other->type;
        std::strcpy(user, other->user);
        buffer_len = other->buffer_len;
        buffer = (char*)std::malloc(buffer_len);
        std::memcpy(buffer, other->buffer, buffer_len);
        buffer_pos = other->buffer_pos;
    }

    void ensure_sized(unsigned int size) {
        if (size > buffer_len) {
            while (buffer_len < size) {
                buffer_len *= 2;
            }
            buffer = (char*)std::realloc(buffer, buffer_len);
        }
    }

    void set_username(const char* name) {
        int p = std::min((int)std::strlen(name), 31);
        std::strncpy(user, name, p);
        user[p] = '\0';
    }

    void store_bytes(const void* data, int len) {
        ensure_sized(buffer_pos + len);
        std::memcpy(buffer + buffer_pos, data, len);
        buffer_pos += len;
    }

    void load_bytes(void* data, unsigned int len) {
        if (buffer_pos != 0) {
            int p = std::min(len, buffer_pos);
            std::memcpy(data, buffer, p);
            buffer_pos -= p;
            std::memmove(buffer, buffer + p, buffer_pos);
        }
    }

    void store_string(const char* str) {
        store_bytes(str, (int)std::strlen(str) + 1);
    }

    void load_str(char* str, unsigned int maxlen) {
        maxlen = std::min(maxlen, (unsigned int)std::strlen(buffer) + 1);
        maxlen = std::min(maxlen, buffer_pos + 1);
        load_bytes(str, maxlen);
        str[maxlen] = '\0';
    }

    void store_int(unsigned int x) {
        store_bytes(&x, 4);
    }

    int load_int() {
        int x = 0;
        load_bytes(&x, 4);
        return x;
    }

    void store_short(unsigned short x) {
        store_bytes(&x, 2);
    }

    unsigned short load_short() {
        unsigned short x = 0;
        load_bytes(&x, 2);
        return x;
    }

    void store_char(unsigned char x) {
        store_bytes(&x, 1);
    }

    unsigned char load_char() {
        unsigned char x = 0;
        load_bytes(&x, 1);
        return x;
    }

    int write_to_message(char* dest, unsigned int max_len) {
        *dest = (char)type;
        int eax = (int)std::strlen(user) + 2;
        std::strcpy(dest + 1, user);
        int ebx = std::min(max_len - eax, buffer_pos);
        std::memcpy(dest + eax, buffer, ebx);
        return eax + ebx;
    }

    void read_from_message(char* p_buffer, int p_buffer_len) {
        type = (INSTRUCTION)(unsigned char)*p_buffer++;

        unsigned int ul = (int)std::strlen(p_buffer);
        int px = std::min(ul, 31u);

        std::memcpy(user, p_buffer, px + 1);
        user[px] = '\0';
        p_buffer += ul + 1;

        p_buffer_len -= (ul + 2);  // +1 for type, +1 for null terminator

        ensure_sized(p_buffer_len);
        std::memcpy(buffer, p_buffer, p_buffer_len);
        buffer_pos = p_buffer_len;
    }
};
