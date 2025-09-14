#ifndef BASE_BUFFER_UNBOUNDEDBUFFER_H
#define BASE_BUFFER_UNBOUNDEDBUFFER_H

#include <cstddef>
#include <vector>

namespace tinyredis {
class UnboundedBuffer {
 public:
  UnboundedBuffer() : readPos_(0), writePos_(0) {}
  std::size_t write(const void* pData, std::size_t nSize);
  std::size_t pushDataAt(const void* pData, std::size_t nSize,
                         std::size_t offset = 0);
  std::size_t pushData(const void* pData, std::size_t nSize);
  std::size_t peekDataAt(void* pBuf, std::size_t nSize, std::size_t offset = 0);
  std::size_t peekData(void* pBuf, std::size_t nSize);
  std::size_t isEmpty() const { return readableSize() == 0; }
  char* readAddr() { return &buffer_[readPos_]; }
  char* writeAddr() { return &buffer_[writePos_]; }
  std::size_t readableSize() const { return writePos_ - readPos_; }
  std::size_t writableSize() const { return buffer_.size() - writePos_; }
  void adjustWritePtr(std::size_t nBytes) { writePos_ += nBytes; }
  void adjustReadPtr(std::size_t nBytes) { readPos_ += nBytes; }

  void swap(UnboundedBuffer& other);
  static const std::size_t MAX_BUFFER_SIZE;

 private:
  void _AssureSpace(std::size_t nSize);
  std::size_t writePos_;
  std::size_t readPos_;
  std::vector<char> buffer_;
};
}  // namespace tinyredis

#endif