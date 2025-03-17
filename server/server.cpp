#include "../shared/util.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cerrno>

constexpr auto k_max_msg = Util::k_max_msg;
constexpr auto &write_all = Util::write_all;
constexpr auto &read_full = Util::read_full;

static void msg(const char *msg) { fprintf(stderr, "%s\n", msg); }

static void die(const char *msg) {
  int err = errno;
  fprintf(stderr, "[%d] %s\n", err, msg);
  abort();
}

static void msg_errno(const char *msg) {
  fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

static void fd_set_nb(int fd) {
  errno = 0;
  int flags = fcntl(fd, F_GETFL, 0);
  if (errno) {
    die("fcntl error");
    return;
  }

  flags |= O_NONBLOCK;

  errno = 0;
  (void)fcntl(fd, F_SETFL, flags);
  if (errno) {
    die("fcntl error");
  }
}

struct Conn {
  int fd = -1;
  // application's intention, for the event loop
  bool want_read = false;
  bool want_write = false;
  bool want_close = false;
  // buffered input and output
  std::vector<uint8_t> incoming; // data to be parsed by the application
  std::vector<uint8_t> outgoing; // responses generated by the application
};

// append to the back
static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data,
                       size_t len) {
  buf.insert(buf.end(), data, data + len);
}

// remove from the front
static void buf_consume(std::vector<uint8_t> &buf, size_t n) {
  buf.erase(buf.begin(), buf.begin() + n);
}

// application callback when the listening socket is ready
static Conn *handle_accept(int fd) {
  // accept
  struct sockaddr_in client_addr = {};
  socklen_t socklen = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
  if (connfd < 0) {
    msg_errno("accept() error");
    return NULL;
  }
  uint32_t ip = client_addr.sin_addr.s_addr;
  fprintf(stdout, "new client from %u.%u.%u.%u:%u\n", ip & 255, (ip >> 8) & 255,
          (ip >> 16) & 255, ip >> 24, ntohs(client_addr.sin_port));

  // set the new connection fd to nonblocking mode
  fd_set_nb(connfd);

  // create a `struct Conn`
  Conn *conn = new Conn();
  conn->fd = connfd;
  conn->want_read = true;
  return conn;
}

// process 1 request if there is enough data
static bool try_one_request(Conn *conn) {
  // try to parse the protocol: message header
  if (conn->incoming.size() < 4) {
    return false; // want read
  }
  uint32_t len = 0;
  memcpy(&len, conn->incoming.data(), 4);
  if (len > k_max_msg) {
    msg("too long");
    conn->want_close = true;
    return false; // want close
  }
  // message body
  if (4 + len > conn->incoming.size()) {
    return false; // want read
  }
  const uint8_t *request = &conn->incoming[4];

  // got one request, do some application logic
  printf("client says: len:%d data:%.*s\n", len, len < 100 ? len : 100,
         request);

  // generate the response (echo)
  buf_append(conn->outgoing, (const uint8_t *)&len, 4);
  buf_append(conn->outgoing, request, len);

  // application logic done! remove the request message.
  buf_consume(conn->incoming, 4 + len);
  // Q: Why not just empty the buffer? See the explanation of "pipelining".
  return true; // success
}

// application callback when the socket is writable
static void handle_write(Conn *conn) {
  assert(conn->outgoing.size() > 0);
  ssize_t rv = write(conn->fd, &conn->outgoing[0], conn->outgoing.size());
  if (rv < 0 && errno == EAGAIN) {
    return; // actually not ready
  }
  if (rv < 0) {
    msg_errno("write() error");
    conn->want_close = true; // error handling
    return;
  }

  // remove written data from `outgoing`
  buf_consume(conn->outgoing, (size_t)rv);

  // update the readiness intention
  if (conn->outgoing.size() == 0) { // all data written
    conn->want_read = true;
    conn->want_write = false;
  } // else: want write
}

// application callback when the socket is readable
static void handle_read(Conn *conn) {
  // read some data
  uint8_t buf[64 * 1024];
  ssize_t rv = read(conn->fd, buf, sizeof(buf));
  if (rv < 0 && errno == EAGAIN) {
    return; // actually not ready
  }
  // handle IO error
  if (rv < 0) {
    msg_errno("read() error");
    conn->want_close = true;
    return; // want close
  }
  // handle EOF
  if (rv == 0) {
    if (conn->incoming.size() == 0) {
      msg("client closed");
    } else {
      msg("unexpected EOF");
    }
    conn->want_close = true;
    return; // want close
  }
  // got some new data
  buf_append(conn->incoming, buf, (size_t)rv);

  // parse requests and generate responses
  while (try_one_request(conn)) {
  }
  // Q: Why calling this in a loop? See the explanation of "pipelining".

  // update the readiness intention
  if (conn->outgoing.size() > 0) { // has a response
    conn->want_read = false;
    conn->want_write = true;
    // The socket is likely ready to write in a request-response protocol,
    // try to write it without waiting for the next iteration.
    return handle_write(conn);
  } // else: want read
}

// Function to update kqueue event subscriptions
static void update_events(int fd, Conn *conn , int kq) {
  struct kevent evSet[2];
  int nev = 0;

  // Always monitor for errors
  EV_SET(&evSet[nev++], fd, EVFILT_READ, EV_ADD | (conn->want_read ? EV_ENABLE : EV_DISABLE), 0, 0, conn);
  EV_SET(&evSet[nev++], fd, EVFILT_WRITE, EV_ADD | (conn->want_write ? EV_ENABLE : EV_DISABLE), 0, 0, conn);

  if (kevent(kq, evSet, nev, NULL, 0, NULL) == -1) {
      perror("kevent");
      exit(1);
  }
}

int main() {
  // the listening socket
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    die("socket()");
  }
  int val = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  // bind
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(0); // wildcard address 0.0.0.0
  int rv = bind(listen_fd, (const sockaddr *)&addr, sizeof(addr));
  if (rv) {
    die("bind()");
  }

  // set the listen fd to nonblocking mode
  fd_set_nb(listen_fd);

  // listen
  rv = listen(listen_fd, SOMAXCONN);
  if (rv) {
    die("listen()");
  }

  // Create kqueue
  int kq = kqueue();
  if (kq == -1) {
    perror("kqueue");
    return 1;
  }

  // Create event for listening socket
  struct kevent evSet;
  EV_SET(&evSet, listen_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
  if (kevent(kq, &evSet, 1, NULL, 0, NULL) == -1) {
    perror("kevent");
    return 1;
  }

  // a map of all client connections, keyed by fd
  std::vector<Conn *> fd2conn; // using vector for performance reasons
  // the event loop
  while (true) {
    struct kevent events[64];
    int nev = kevent(kq, NULL, 0, events, 64, NULL);
    if (nev < 0) {
        if (errno == EINTR) continue;
        perror("kevent");
        break;
    }

    for (int i = 0; i < nev; i++) {
        int fd = events[i].ident;
        int filter = events[i].filter;

        if (fd == listen_fd && filter == EVFILT_READ) {
            // Accept new connection
            Conn *conn = handle_accept(listen_fd);
            if (conn) {
                if ((size_t)conn->fd >= fd2conn.size()) {
                    fd2conn.resize(conn->fd + 1);
                }
                fd2conn[conn->fd] = conn;
                update_events(conn->fd, conn, kq);
            }
        } else {
            // Handle client connections
            Conn *conn = fd2conn[fd];
            if (!conn) continue;

            if (filter == EVFILT_READ) {
                handle_read(conn);
            } 
            if (filter == EVFILT_WRITE) {
                handle_write(conn);
            }

            if (conn->want_close) {
                close(fd);
                fd2conn[fd] = nullptr;
                delete conn;
            } else {
                update_events(fd, conn, kq);
            }
        }
    }
}
  close(kq);
  return 0;
}
