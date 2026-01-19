#pragma once

#include <cstdlib>
#include <cstring>
#include <algorithm>

class k_framecache {
public:
    char* buffer;
    int pos;
    int size;

    k_framecache() : buffer(nullptr), pos(0), size(0) {}

    k_framecache(const char* buf, int len) : pos(len), size(len) {
        buffer = (char*)std::malloc(len);
        std::memcpy(buffer, buf, len);
    }

    ~k_framecache() {
        if (buffer != nullptr) {
            std::free(buffer);
            buffer = nullptr;
            pos = 0;
            size = 0;
        }
    }

    // Non-copyable
    k_framecache(const k_framecache&) = delete;
    k_framecache& operator=(const k_framecache&) = delete;

    // Movable
    k_framecache(k_framecache&& other) noexcept
        : buffer(other.buffer), pos(other.pos), size(other.size) {
        other.buffer = nullptr;
        other.pos = 0;
        other.size = 0;
    }

    k_framecache& operator=(k_framecache&& other) noexcept {
        if (this != &other) {
            std::free(buffer);
            buffer = other.buffer;
            pos = other.pos;
            size = other.size;
            other.buffer = nullptr;
            other.pos = 0;
            other.size = 0;
        }
        return *this;
    }

    void put_data(const void* data, int datalen) {
        if (datalen > 0) {
            ensure_sized(pos + datalen);
            std::memcpy(buffer + pos, data, datalen);
            pos += datalen;
        }
    }

    int peek_data(char* datab, int len) {
        int x = std::min(len, pos);
        if (x > 0) {
            std::memcpy(datab, buffer, x);
            return x;
        }
        return 0;
    }

    void ensure_sized(int datalen) {
        if (buffer == nullptr) {
            size = datalen * 6;
            buffer = (char*)std::malloc(size);
        } else if (datalen > size) {
            size = datalen * 2;
            buffer = (char*)std::realloc(buffer, size);
        }
    }

    int get_data(char* datab, int len) {
        int x = std::min(len, pos);
        if (x > 0) {
            std::memcpy(datab, buffer, x);
            pos -= x;
            std::memmove(buffer, buffer + x, pos);
            return x;
        }
        return 0;
    }

    void reset() {
        pos = 0;
    }

    int available() const {
        return pos;
    }
};
