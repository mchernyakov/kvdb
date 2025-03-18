#include "util.h"
#include <string>

int32_t Util::read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error, or unexpected EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

int32_t Util::write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

std::string Util::get_result_code(uint32_t value) {
    switch (value) {
        case RES_OK: return "RES_OK";
        case RES_ERR: return "RES_ERR";
        case RES_NX: return "RES_NX";
        default: return "UNKNOWN"; // Handle unexpected values
    }
}
