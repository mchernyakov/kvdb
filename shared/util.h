#ifndef UTIL_H
#define UTIL_H

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <unistd.h>

class Util {
public:
    static constexpr size_t k_max_msg = 2 << 12;
    static constexpr size_t k_max_args = 200 * 1000;

    static int32_t read_full(int fd, char *buf, size_t n);
    static int32_t write_all(int fd, const char *buf, size_t n);
};

#endif  // UTIL_H
