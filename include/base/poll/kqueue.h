#ifndef BASE_POLL_KQUEUE_H
#define BASE_POLL_KQUEUE_H

#include <base/poll/poller.h>
#include <vector>

#if defined(__APPLE__)

class Kqueue : public Poller {
 public:
  Kqueue();
  ~Kqueue();

  bool addSocket(int sock, int events, void* userPtr) override;
  bool modSocket(int sock, int events, void* userPtr) override;
  bool delSocket(int sock, int events) override;

  int poll(std::vector<FiredEvent>& events, std::size_t maxEv,
           int timeOutMs) override;

 private:
  std::vector<struct kevent> events_;
};

#endif  // __APPLE__
#endif
