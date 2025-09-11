#ifndef BASE_SOCKET_SOCKET_H
#define BASE_SOCKET_SOCKET_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/_endian.h>
#include <sys/_types/_socklen_t.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>

#define INVALID_SOCKET (int)(~0)  // 0xFFFFFFFF
#define SOCKET_ERROR -1
#define INVALID_PORT -1

struct SocketAddr {
  SocketAddr() { clear(); }
  SocketAddr(const SocketAddr& other) {
    memcpy(&addr_, &other.addr_, sizeof addr_);
  }
  SocketAddr& operator=(const SocketAddr& other) {
    if (this != &other) {
      memcpy(&addr_, &other.addr_, sizeof(addr_));
    }
    return *this;
  }
  SocketAddr(const sockaddr_in& addr) { init(addr); }

  // ip_port format: 127.0.0.1:6379
  SocketAddr(const std::string& ip_port) {
    std::string::size_type p = ip_port.find_first_of(':');
    std::string ip = ip_port.substr(0, p);
    std::string port = ip_port.substr(p + 1);

    init(ip.c_str(), static_cast<uint16_t>(std::stoi(port)));
  }

  const sockaddr_in& getAddr() const { return addr_; }

  const char* getIP() const {
    return ::inet_ntoa(addr_.sin_addr);
    // inet_ntoa参数：struct in_addr
    // inet_ntoa作用：网络字节序 -> 字符串ip
  }

  const char* getIP(char* buffer, socklen_t size) const {
    return ::inet_ntop(AF_INET, &addr_.sin_addr, buffer, size);
    // 转换后的字符串将写入到 buffer
  }

  uint16_t getPort() const { return ntohs(addr_.sin_port); }

  std::string toString() const {
    char tmp
        [32];  // IPv4字符串占15字节，'\0'占1字节，最少需要16字节，32为安全冗余
    const char* ip = getIP(tmp, (socklen_t)sizeof(tmp));  // 复用GetIP函数
    return std::string(ip) + ":" + std::to_string(ntohs(addr_.sin_port));
  }

  void init(const char* ip, uint16_t host_port) {
    // uint16_t: 16 位的无符号整数类型:0-65535
    addr_.sin_family = AF_INET;               // AF_INET：IPv4
    addr_.sin_addr.s_addr = ::inet_addr(ip);  // ip字符串转换为网络字节序
    addr_.sin_port = htons(host_port);  // 端口号转换为网络字节序 (h to n s)
  }

  void init(sockaddr_in addr) { memcpy(&addr_, &addr, sizeof(addr_)); }
  void clear() { memset(&addr_, 0, sizeof(addr_)); }
  bool empty() const { return addr_.sin_family == 0; }  // clear()后为0

  // 不使用 friend 时，编译器会默认将第一个参数视为this指针
  // 不使用 friend 时，运算符重载函数会被视为类的成员函数
  // 声明为friend的函数不是类的成员函数，而是具有特殊访问权限的全局函数
  friend bool operator==(const SocketAddr& a, const SocketAddr& b) {
    return a.addr_.sin_family == b.addr_.sin_family &&
           a.addr_.sin_addr.s_addr == b.addr_.sin_addr.s_addr &&
           a.addr_.sin_port == b.addr_.sin_port;
  }

  friend bool operator!=(const SocketAddr& a, const SocketAddr& b) {
    return !(a == b);
  }
  sockaddr_in addr_;
};

// 标准库模板 std::hash 的特化
namespace std {
template <>
struct hash<SocketAddr> {
  using argument_type = SocketAddr;
  // 无符号类型，std::hash 要求哈希结果必须是 size_t
  using result_type = std::size_t;

  result_type operator()(const argument_type& s) const noexcept {
    result_type h1 = std::hash<short>{}(s.addr_.sin_family);
    result_type h2 = std::hash<uint16_t>{}(s.addr_.sin_port);
    result_type h3 = std::hash<uint32_t>{}(s.addr_.sin_addr.s_addr);
    result_type tmp = h1 ^ (h2 << 1);
    return h3 ^ (tmp << 1);
  }

  // 例子 std::unordered_set<SocketAddr> addrSet
  // 插入 SocketAddr 对象时，容器会自动调用你定义的 hash<SocketAddr>::operator()，传入该对象作为参数。
};
}  // namespace std

namespace Internal {
class SendThread;
}

// 能够使用 shared_from_this() 方法返回指向自身的 shared_ptr
class Socket : public std::enable_shared_from_this<Socket> {
  friend class Internal::SendThread;
  // 允许 Internal::SendThread 类访问 Socket 的所有成员

 public:
  virtual ~Socket();

  // 禁用拷贝构造与赋值
  Socket(const Socket&) = delete;
  void operator=(const Socket&) = delete;

  std::size_t getID() const { return id_; }

  virtual bool OnReadable() { return false; }
  virtual bool OnWritable() { return false; }
  virtual bool OnError();
  virtual bool OnConnect();
  virtual bool OnDisconnect();

  static void closeSocket(int& sock);
  static int createUDPSocket();
  static int createTCPSocket();
  static void setNonBlock(int sock,bool nonblock);
  static void setNodelay(int sock);

  enum class SocketType {
    invalid,
    listen,  // 监听套接字（服务器端）
    client,  // 客户端套接字
    stream   // 流式套接字（如TCP）
  };

  virtual SocketType getSocketType() const { return SocketType::invalid; }
  bool invalid() const { return invalid_; }
  int getSocket() const { return localSock_; }

 protected:
  Socket();
  int localSock_;
  bool epollOut_;

 private:
  std::atomic<bool> invalid_;
  std::size_t id_;

  // 全局自增计数器，通过自增操作确保每个新对象获得不同的 ID
  static std::atomic<std::size_t> sid_;  // static id
};

#endif