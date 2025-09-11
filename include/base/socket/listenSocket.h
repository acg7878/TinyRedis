#ifndef BASE_SOCKET_LISTENSOCKET_H
#define BASE_SOCKET_LISTENSOCKET_H

#include <base/socket/socket.h>
#include <netinet/in.h>
#include <cstdint>

namespace Internal {
class ListenSocket : public Socket {
  static const int LISTENQ;  //  TCP 监听队列的最大长度

 public:
  explicit ListenSocket(int tag);
  ~ListenSocket();

  SocketType getSocketType() const { return SocketType::listen; }

  bool Bind(const SocketAddr& addr);
  bool OnReadable();
  bool OnWritable();
  bool OnError();

 private:
  int _Accept();
  sockaddr_in addrClient_;
  uint16_t localPort_;
  const int tag_;
};
}  // namespace Internal

#endif
