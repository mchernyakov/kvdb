#ifndef UTIL_H
#define UTIL_H

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <unistd.h>

class Util {
public:
    static constexpr size_t k_max_msg = 32 << 20;

    static int32_t read_full(int fd, uint8_t *buf, size_t n);
    static int32_t write_all(int fd, const uint8_t *buf, size_t n);
};

#endif  // UTIL_H
