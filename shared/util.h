#ifndef UTIL_H
#define UTIL_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unistd.h>

typedef enum {
    RES_OK = 0,
    RES_ERR = 1, // error
    RES_NX = 2,  // key not found
} ResultCode;

class Util {
public:
  static constexpr size_t k_max_msg_server = 32 << 20;
  static constexpr size_t k_max_msg_client = 2 << 12;
  static constexpr size_t k_max_args = 200 * 1000;

  static int32_t read_full(int fd, char *buf, size_t n);
  static int32_t write_all(int fd, const char *buf, size_t n);

  static std::string get_result_code(uint32_t value);
};

#endif // UTIL_H
