#ifndef BASE_POLL_POLLER_H
#define BASE_POLL_POLLER_H

#include <vector>
#include <cstddef>

enum class EventType {
  Read  = 0x1,
  Write = 0x1 << 1,
  Error = 0x1 << 2,
};

struct FiredEvent {
  int events{0};
  void* userdata{nullptr};
};

class Poller {
 public:
  Poller() = default;
  virtual ~Poller() = default;

  virtual bool addSocket(int sock, int events, void* userPtr) = 0;
  virtual bool modSocket(int sock, int events, void* userPtr) = 0;
  virtual bool delSocket(int sock, int events) = 0;

  virtual int poll(std::vector<FiredEvent>& events, std::size_t maxEv,
                   int timeOutMs) = 0;

 protected:
  int multiplexer_{-1};
};

#endif
