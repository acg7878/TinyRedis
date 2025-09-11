#ifndef BASE_SOCKET_CLIENTSOCKET_H
#define BASE_SOCKET_CLIENTSOCKET_H

#include <base/socket/socket.h>
#include <functional>

class ClientSocket : public Socket {
 public:
  explicit ClientSocket(int tag);
  ~ClientSocket();
  bool Connect(const SocketAddr& addr);
  bool OnWritable();
  bool OnError();
  SocketType getSockType() const { return SocketType::client; }

  void setFailCallback(const std::function<void()>& cb) { onConnectFail_ = cb; }

 private:
  const int tag_;
  SocketAddr peerAddr_;
  std::function<void()> onConnectFail_;
};

#endif