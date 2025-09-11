#if defined(__APPLE__)

#include <base/poll/kqueue.h>
#include <base/poll/poller.h>
#include <spdlog/spdlog.h>
#include <sys/event.h>
#include <unistd.h>
#include <cerrno>
#include <stdexcept>

Kqueue::Kqueue() {
  multiplexer_ = ::kqueue();
  if (multiplexer_ == -1) {
    spdlog::error("Failed to create kqueue: {}", strerror(errno));
    throw std::runtime_error("Failed to create kqueue");
  }
  events_.reserve(64);
  spdlog::info("create kqueue: {}", multiplexer_);
}

Kqueue::~Kqueue() {
  spdlog::info("close kqueue: {}", multiplexer_);
  if (multiplexer_ != -1) {
    ::close(multiplexer_);
  }
}

bool Kqueue::addSocket(int sock, int events, void* userPtr) {
  struct kevent change[2];  // 事件，描述被监控的事件类型和状态
  int cnt = 0;

  // events 是一个整型的事件掩码
  if (events & static_cast<int>(EventType::Read)) {
    // EV_ADD:添加事件）
    EV_SET(&change[cnt++], sock, EVFILT_READ, EV_ADD, 0, 0, userPtr);
  }

  if (events & static_cast<int>(EventType::Write)) {
    EV_SET(&change[cnt++], sock, EVFILT_WRITE, EV_ADD, 0, 0, userPtr);
  }

  int ret = ::kevent(multiplexer_, change, cnt, nullptr, 0, nullptr);
  if (ret == -1) {
    spdlog::error("addSocket failed (fd {}): {}", sock, strerror(errno));
    return false;
  }
  return true;
}

bool Kqueue::delSocket(int sock, int events) {
  struct kevent change[2];
  int cnt = 0;

  if (events & static_cast<int>(EventType::Read)) {
    EV_SET(&change[cnt++], sock, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
  }

  if (events & static_cast<int>(EventType::Write)) {
    EV_SET(&change[cnt++], sock, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
  }

  if (cnt == 0)
    return false;

  int ret = ::kevent(multiplexer_, change, cnt, nullptr, 0, nullptr);
  if (ret == -1) {
    spdlog::warn("delSocket failed (fd {}): {}", sock, strerror(errno));
    return false;
  }
  return true;
}

bool Kqueue::modSocket(int sock, int events, void* userPtr) {
  // 删除该套接字上所有已注册的读/写事件
  bool ret = delSocket(sock, static_cast<int>(EventType::Read) |
                                 static_cast<int>(EventType::Write));
  if (events == 0)
    return ret;
  return addSocket(sock, events, userPtr);
}

int Kqueue::poll(std::vector<FiredEvent>& firedEvents, std::size_t maxEvent,
                 int timeoutMs) {
  if (maxEvent == 0)  // 最大接受事件数
    return 0;

  if (events_.size() < maxEvent)
    events_.resize(maxEvent);

  struct timespec timeout;              // 用于存储超时时间的结构体
  struct timespec* pTimeout = nullptr;  // 指向超时时间的指针
  if (timeoutMs >= 0) {
    timeout.tv_sec = timeoutMs / 1000;               // 秒数
    timeout.tv_nsec = (timeoutMs % 1000) * 1000000;  // 纳秒数
    pTimeout = &timeout;
  }

  int nFired = ::kevent(multiplexer_, nullptr, 0, events_.data(),
                        static_cast<int>(maxEvent), pTimeout);
  if (nFired == -1) {
    if (errno == EINTR)
      return 0;  // 被信号打断，不报错
    spdlog::error("kevent poll failed: {}", strerror(errno));
    return -1;
  }

  firedEvents.clear();
  firedEvents.reserve(nFired);

  for (int i = 0; i < nFired; ++i) {
    FiredEvent fe;
    fe.events = 0;
    fe.userdata = events_[i].udata;

    // |= 按位或赋值运算符
    if (events_[i].filter == EVFILT_READ)
      fe.events |= static_cast<int>(EventType::Read);
    if (events_[i].filter == EVFILT_WRITE)
      fe.events |= static_cast<int>(EventType::Write);
    if (events_[i].flags & EV_ERROR)
      fe.events |= static_cast<int>(EventType::Error);

    firedEvents.push_back(fe);
  }

  return nFired;  // 发生的事件数
}

#endif
