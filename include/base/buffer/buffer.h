#ifndef BASE_BUFFER_BUFFER_H
#define BASE_BUFFER_BUFFER_H

#include <sys/uio.h>
#include <atomic>
#include <cassert>
#include <cstddef>

struct BufferSequence {
  static const std::size_t kMaxIovec = 16;
  iovec buffers[kMaxIovec];
  std::size_t count;

  // 	计算所有有效内存块的总字节数
  std::size_t totalBytes() const {
    assert(count <= kMaxIovec);
    std::size_t nBytes = 0;
    for (std::size_t i = 0; i < count; ++i) {
      nBytes += buffers[i].iov_len;
    }
    return nBytes;
  }
};

// 以 2 为幂找到一个大于等于 size 的数（如：输入5，找到2->4->8）
inline std::size_t roundUp2Power(std::size_t size) {
  if (size == 0)
    return 0;

  std::size_t roundSize = 1;
  while (roundSize < size)
    roundSize <<= 1;

  return roundSize;
}

template <typename BUFFER>
class CircularBuffer {
 public:
  CircularBuffer(std::size_t size = 0)
      : maxSize_(size), readPos_(0), writePos_(0) {}
  CircularBuffer(const BufferSequence& bf);
  CircularBuffer(char*, std::size_t);
  ~CircularBuffer() {}

  bool isEmpty() const { return readPos_ = writePos_; }

  // (writePos_ + 1) & (maxSize_ - 1) 相当于取模了，因为maxSize_ - 1为全1
  // maxSize_是2的n次方，如8，那8-1为7（111）
  bool isFull() const {return ((writePos_+1)&(maxSize_-1)) == readPos_;}

  
 protected:
  std::size_t maxSize_;

 private:
  BUFFER buffer;
  std::atomic<std::size_t> readPos_;   // 可以读的开始处
  std::atomic<std::size_t> writePos_;  // 可以写的开始处
  bool owned_ = false;
};
#endif