#include <base/socket/socket.h>
#include <sys/socket.h>
#include <unistd.h>

Socket::Socket()
    : localSock_(INVALID_SOCKET), epollOut_(false), invalid_(false) {
  ++sid_;
  std::size_t expect = 0;
  sid_.compare_exchange_strong(expect, 1);
  id_ = sid_;
}

void Socket::closeSocket(int& sock) {
  if (sock != INVALID_SOCKET) {
    ::shutdown(sock, SHUT_RDWR);
    ::close(sock);
  }
}