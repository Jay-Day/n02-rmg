#pragma once

#include <cstring>
#include <cstdlib>

// Static-sized list (unordered - swap on remove)
template <class T, int MaxSize>
class slist {
public:
    T items[MaxSize];
    int length = 0;

    void add(T element) {
        if (length < MaxSize) {
            items[length++] = element;
        }
    }

    void removei(int i) {
        if (i >= 0 && i < length) {
            int last = length - 1;
            if (last != i) {
                items[i] = items[last];
            }
            length = last;
        }
    }

    void remove(T t) {
        for (int i = 0; i < length; i++) {
            if (items[i] == t) {
                removei(i);
                i--;
            }
        }
    }

    void set(T v, int i) { items[i] = v; }
    T get(int i) { return items[i]; }
    T operator[](int i) { return items[i]; }
    void clear() { length = 0; }
    int size() { return length; }
};

// Ordered static-sized list (shift on remove to maintain order)
template <class T, int MaxSize>
class oslist {
public:
    T items[MaxSize];
    int length = 0;

    void add(T element) {
        if (length < MaxSize) {
            items[length++] = element;
        }
    }

    void removei(int i) {
        if (i >= 0 && i < length) {
            int last = length - 1;
            if (last != i) {
                std::memmove(&items[i], &items[i + 1], (last - i) * sizeof(T));
            }
            length = last;
        }
    }

    void remove(T t) {
        for (int i = 0; i < length; i++) {
            if (items[i] == t) {
                removei(i);
                i--;
            }
        }
    }

    void set_size(int l) { length = l; }
    void set(T v, int i) { items[i] = v; }
    T get(int i) { return items[i]; }
    T operator[](int i) { return items[i]; }
    void clear() { length = 0; }
    int size() { return length; }
};

// Dynamic ordered list (heap-allocated, grows as needed)
template <class T, int MinLen = 32>
class odlist {
public:
    T* items = nullptr;
    int length = 0;

    odlist() = default;

    ~odlist() {
        if (items != nullptr) {
            std::free(items);
            items = nullptr;
            length = 0;
        }
    }

    // Non-copyable
    odlist(const odlist&) = delete;
    odlist& operator=(const odlist&) = delete;

    void add(T element) {
        if (items == nullptr) {
            items = (T*)std::malloc(MinLen * sizeof(T));
        } else if (length >= MinLen) {
            items = (T*)std::realloc(items, (length + 1) * sizeof(T));
        }
        items[length++] = element;
    }

    void removei(int i) {
        if (i >= 0 && i < length) {
            int last = length - 1;
            if (last != i) {
                std::memmove(&items[i], &items[i + 1], (last - i) * sizeof(T));
            }
            length = last;
            if (last >= MinLen) {
                items = (T*)std::realloc(items, last * sizeof(T));
            }
        }
    }

    void remove(T t) {
        for (int i = 0; i < length; i++) {
            if (items[i] == t) {
                removei(i);
                break;
            }
        }
    }

    void set(T v, int i) {
        if (i >= 0 && i < length) {
            items[i] = v;
        }
    }

    T get(int i) { return (i >= 0 && i < length) ? items[i] : T{}; }
    T operator[](int i) { return items[i]; }

    void clear() {
        length = 0;
        if (items != nullptr) {
            items = (T*)std::realloc(items, MinLen * sizeof(T));
        }
    }

    int size() { return length; }
};
