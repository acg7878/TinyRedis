#ifndef BASE_BUFFER_ASYNCBUFFER
#define BASE_BUFFER_ASYNCBUFFER

#include <base/buffer/buffer.h>
#include <base/buffer/unboundedBuffer.h>
#include <atomic>
#include <cstddef>
#include <mutex>
class AsyncBuffer {
 public:
  explicit AsyncBuffer(std::size_t size = 128 * 1024);
  ~AsyncBuffer();

  void write(const void* data, std::size_t len);
  void write(const BufferSequence& data);
  void processBuffer(BufferSequence& data);
  void skip(std::size_t size);

 private:
  BUFFER buffer_;
  tinyredis::UnboundedBuffer tmpBuffer_;

  std::mutex backBufferLock_;
  std::atomic<std::size_t> backBytes_;
  tinyredis::UnboundedBuffer backBuffer_;
};

#endif