#ifndef BASE_SOCKET_STREAMSOCKET_H
#define BASE_SOCKET_STREAMSOCKET_H

#include <base/socket/socket.h>

using packetLength = int32_t;

class StreamSocket : public Socket {
 public:
  StreamSocket();
  ~StreamSocket();

  bool init(int localfd, const SocketAddr& peer);
  SocketType getSocketType() { return SocketType::stream; }
  bool DoMsgParse();
  const SocketAddr& getPeerAddr() const { return peerAddr_; }

 public:
  int recv();

 protected:
  SocketAddr peerAddr_;
};

#endif