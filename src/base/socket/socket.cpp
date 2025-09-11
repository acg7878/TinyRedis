#include <base/socket/socket.h>
#include <netinet/tcp.h>
#include <spdlog/spdlog.h>
#include <sys/fcntl.h>
#include <sys/socket.h>

#if defined(__APPLE__)
#include <unistd.h>
#endif

Socket::Socket()
    : localSock_(INVALID_SOCKET), epollOut_(false), invalid_(false) {
  ++sid_;
  std::size_t expect = 0;
  sid_.compare_exchange_strong(expect, 1);
  id_ = sid_;
}

Socket::~Socket() {
  closeSocket(localSock_);
}

// 创建一个 UDP 套接字
int Socket::createUDPSocket() {
  return ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  // ::socket(地址族,套接字类型,协议类型)
}

int Socket::createTCPSocket() {
  return ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

/*
检查当前套接字是否已处于 “无效 / 错误状态”，
如果未处于错误状态，则将其标记为错误状态，并返回 true；
如果已处于错误状态，则直接返回 false*
*/
bool Socket::OnError() {
  bool expect = false;
  // 看看 invalid_ 是不是和 expect相等，相等就将 invalid_ 设置为true
  if (invalid_.compare_exchange_strong(expect, true)) {
    return true;
  }
  return false;
}

void Socket::closeSocket(int& sock) {
  if (sock != INVALID_SOCKET) {
    ::shutdown(sock, SHUT_RDWR);  // 关闭套接字读写方向
    ::close(sock);                // 释放套接字资源
    spdlog::debug("closeSocket {}", sock);
    sock = INVALID_SOCKET;
  }
}

void Socket::setNonBlock(int sock, bool nonblock) {
  // 获取套接字（或文件描述符）的当前状态标志
  // F_GETFL : 获取文件状态标志（File GET Flag List）
  // 0：这个命令不需要额外参数，传 0 即可。
  int flag = ::fcntl(sock, F_GETFL, 0);
  if (nonblock) {
    flag = ::fcntl(sock, F_SETFL, flag | O_NONBLOCK);
    // 非阻塞
  } else {
    flag = ::fcntl(sock, F_SETFL, flag & ~O_NONBLOCK);
    // 阻塞
  }
}

// 禁用 TCP 协议中 Nagle 算法
void Socket::setNodelay(int sock) {
  int nodelay = 1;
  ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay,
               sizeof(int));
  // setsockopt(sock，协议，设置的具体选项，指向选项值的指针，选项值的大小)
}