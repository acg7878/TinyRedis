#ifndef BASE_BUFFER_BUFFER_H
#define BASE_BUFFER_BUFFER_H

#include <sys/uio.h>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

// 封装的iovec
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
  bool isFull() const { return ((writePos_ + 1) & (maxSize_ - 1)) == readPos_; }

  void getDatum(BufferSequence& buffer, std::size_t maxSize,
                std::size_t offset = 0);
  void getSpace(BufferSequence& buffer, std::size_t offset = 0);
  bool pushData(const void* pData, std::size_t nSize);
  bool pushDataAt(const void* pData, std::size_t nSize, std::size_t offset = 0);
  bool peekData(void* pBuf, std::size_t nSize);
  bool peekDataAt(void* pBuf, std::size_t nSizeƒ, std::size_t offset = 0);

  char* readAddr() { return &buffer_[readPos_]; }
  char* writeAddr() { return &buffer_[writePos_]; }

  void adjustReadAddr(std::size_t size);
  void adjustWriteAddr(std::size_t size);

  std::size_t readableSize() const {
    return (writePos_ - readPos_) & (maxSize_ - 1);
  }
  std::size_t writableSize() const { return maxSize_ - readableSize() - 1; }

  std::size_t capacity() const { return maxSize_; }
  void initCapacity(std::size_t size);

  template <typename T>
  CircularBuffer& operator<<(const T& data);
  template <typename T>
  CircularBuffer& operator>>(T& data);

  template <typename T>
  CircularBuffer& operator<<(const std::vector<T>&);
  template <typename T>
  CircularBuffer& operator>>(std::vector<T>&);

  CircularBuffer& operator<<(const std::string& str);
  CircularBuffer& operator>>(std::string& str);

 protected:
  std::size_t maxSize_;

 private:
  BUFFER buffer_;
  std::atomic<std::size_t> readPos_;   // 可以读的开始处
  std::atomic<std::size_t> writePos_;  // 可以写的开始处
  bool owned_ = false;                 // 标记内存所有权
};

// 获取缓冲区中可读的数据块（供读取操作）
template <typename BUFFER>
void CircularBuffer<BUFFER>::getDatum(BufferSequence& buffer,
                                      std::size_t maxSize, std::size_t offset) {
  if (maxSize == 0 || offset > readableSize()) {
    buffer.count = 0;
    return;
  }
  assert(readPos_ < maxSize_);
  assert(writePos_ < maxSize_);

  std::size_t bufferIndex = 0;
  const std::size_t readPos = (readPos_ + offset) & (maxSize_ - 1);
  const std::size_t writePos = writePos_;
  assert(readPos != writePos);

  buffer.buffers[bufferIndex].iov_base = &buffer_[readPos];
  if (readPos < writePos) {  // 未绕圈的情况
    // maxSize : 本次想读的最大size
    buffer.buffers[bufferIndex].iov_len = std::min(maxSize, writePos - readPos);
  } else {  // 绕圈
    // 处理第一段数据（从readPos到缓冲区末尾）
    std::size_t nLeft = maxSize;
    if (nLeft > (maxSize_ - readPos))
      nLeft = maxSize_ - readPos;
    buffer.buffers[bufferIndex].iov_len = nLeft;
    nLeft = maxSize - nLeft;

    //如果还有剩余需求，且第二段有数据（writePos > 0）
    if (nLeft > 0 && writePos > 0) {
      if (nLeft > writePos) {
        nLeft = writePos;
      }
      ++bufferIndex;
      buffer.buffers[bufferIndex].iov_base =
          &buffer_[0];  // 第二段的起始地址是缓冲区开头
      buffer.buffers[bufferIndex].iov_len = nLeft;
    }
  }
  buffer.count = bufferIndex + 1;
}

//获取缓冲区中可写的空闲空间（供写入操作）
template <typename BUFFER>
void CircularBuffer<BUFFER>::getSpace(BufferSequence& buffer,
                                      std::size_t offset) {
  // 确保读写指针合法
  assert(readPos_ >= 0 && readPos_ < maxSize_);
  assert(writePos_ >= 0 && writePos_ < maxSize_);

  // 若可写空间 <= (偏移量 + 1)，则无法写入有效数据（+1是为了预留安全距离，避免读写指针重叠）
  if (writableSize() <= offset + 1) {
    buffer.count = 0;
    return;
  }
  std::size_t bufferIndex = 0;
  const std::size_t readPos = readPos_;
  const std::size_t writePos = (writePos_ + offset) & (maxSize_ - 1);
  buffer.buffers[bufferIndex].iov_base = &buffer_[writePos];
  // 未绕圈
  if (readPos > writePos) {
    // 可写空间是连续的一段：从writePos到readPos之前（需减1避免与读指针重叠）
    buffer.buffers[bufferIndex].iov_len = readPos - writePos - 1;
    assert(buffer.buffers[bufferIndex].iov_len > 0);
  } else {
    // 情况2：读指针在写指针之前或相等（已绕圈）
    // 第一段可写空间：从writePos到缓冲区末尾
    buffer.buffers[bufferIndex].iov_len = maxSize_ - writePos;
    // 特殊处理：若读指针在起始位置（0），需从第一段长度减1（避免与读指针重叠）
    if (0 == readPos) {
      buffer.buffers[bufferIndex].iov_len -= 1;
    }
    // 若读指针位置大于1，说明缓冲区开头还有第二段可写空间
    else if (readPos > 1) {
      // 切换到下一个缓冲区条目
      ++bufferIndex;
      // 第二段可写空间的起始地址：缓冲区开头（0位置）
      buffer.buffers[bufferIndex].iov_base = &buffer_[0];
      // 第二段可写长度：从0到readPos之前（减1避免与读指针重叠）
      buffer.buffers[bufferIndex].iov_len = readPos - 1;
    }
  }
  // 最终缓冲区序列的实际数量 = 索引 + 1（因索引从0开始计数）
  buffer.count = bufferIndex + 1;
}

template <typename BUFFER>
bool CircularBuffer<BUFFER>::pushDataAt(const void* pData, std::size_t nSize,
                                        std::size_t offset) {
  if (!pData || nSize == 0) {
    return true;
  }
  if (offset + nSize > writableSize())
    return false;

  const std::size_t readPos = readPos_;
  const std::size_t writePos = (writePos_ + offset) & (maxSize_ - 1);

  // 写空间被分为了两半，那么空出一块连续的可写空间
  /*
      下标：0 1 2 3 4 5 6 7
      数据：□ □ ■ ■ ■ ■ □ □  
             ↑         ↑
       writePos=2   readPos=6
       ■表示已写入的数据（但已被读过，变为空闲）
       □表示未写入的空闲空间（可写）
  */
  if (readPos > writePos) {
    assert(readPos - writePos > nSize);
    ::memcpy(&buffer_[writePos], pData, nSize);
  } else {
    std::size_t availBytesOne = maxSize_ - writePos;
    std::size_t availBytesTwo = readPos - 0;

    assert(availBytesOne + availBytesTwo >= 1 + nSize);
    if (availBytesOne >= nSize + 1) {
      ::memcpy(&buffer_[writePos], pData, nSize);
    } else {
      ::memcpy(&buffer_[writePos], pData, availBytesOne);
      int bytesLeft = static_cast<int>(nSize - availBytesOne);
      if (bytesLeft > 0) {
        //void* 指针不能直接加减偏移
        //第二段缓冲区的起始是0开头
        ::memcpy(&buffer_[0], static_cast<const char*>(pData) + availBytesOne,
                 bytesLeft);
      }
    }
  }
  return true;
}

template <typename BUFFER>
bool CircularBuffer<BUFFER>::pushData(const void* pData, std::size_t nSize) {
  if (!pushDataAt(pData, nSize))
    return false;
  adjustWriteAddr(nSize);
  return true;
}

template <typename BUFFER>
void CircularBuffer<BUFFER>::adjustWriteAddr(std::size_t nSize) {
  std::size_t writePos = writePos_;
  writePos += nSize;
  writePos &= maxSize_ - 1;
  writePos_ = writePos;
}

template <typename BUFFER>
bool CircularBuffer<BUFFER>::peekDataAt(void* pBuf, std::size_t nSize,
                                        std::size_t offset) {
  if (!pBuf || nSize == 0) {
    return true;
  }
  if (nSize + offset > readableSize()) {
    return false;
  }
  const std::size_t writePos = writePos_;
  const std::size_t readPos = (readPos_ + offset) % (maxSize_ - 1);

  if (readPos < writePos) {
    assert(writePos - readPos > nSize);
    ::memcpy(pBuf, &buffer_[readPos], nSize);
  } else {
    assert(readPos > writePos);
    std::size_t Byte1 = maxSize_ - readPos;
    std::size_t Byte2 = writePos - 0;
    assert(Byte1 + Byte2 >= nSize);
    if (Byte1 >= nSize) {
      ::memcpy(pBuf, &buffer_[readPos], nSize);
    } else {
      ::memcpy(pBuf, &buffer_[readPos], Byte1);
      assert(nSize - Byte1 > 0);
      ::memcpy(static_cast<char*>(pBuf) + Byte1, &buffer_[0], nSize - Byte1);
    }
  }
  return true;
}

template <typename BUFFER>
bool CircularBuffer<BUFFER>::peekData(void* pBuf, std::size_t nSize) {
  if (!peekDataAt(pBuf, nSize))
    return false;
  adjustReadAddr(nSize);
  return true;
}

template <typename BUFFER>
void CircularBuffer<BUFFER>::adjustReadAddr(std::size_t nSize) {
  std::size_t readPos = readPos_;
  readPos += nSize;
  readPos &= maxSize_ - 1;
  readPos_ = readPos;
}

template <typename BUFFER>
void CircularBuffer<BUFFER>::initCapacity(std::size_t size) {
  //大于0，且不超过1GB
  assert(size > 0 && size <= 1 * 1024 * 1024 * 1024);

  maxSize_ = roundUp2Power(size);
  buffer_.resize(maxSize_);
  // 内存优化：释放vector中可能存在的冗余内存（收缩到实际需要的大小）
  std::vector<char>(buffer_).swap(buffer_);
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER>& CircularBuffer<BUFFER>::operator<<(
    const T& data) {
  //buffer << 10;       // T = int
  //buffer << 3.14;     // T = double
  //buffer << "hello";  // T = const char*
  if (!pushData(&data, sizeof(data))) {
    // assert 只在条件为 false 时生效
    assert(!!!"Please modify the DEFAULT_BUFFER_SIZE");
  }
  return *this;
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER>& CircularBuffer<BUFFER>::operator>>(T& data) {
  if (!peekData(&data, sizeof(data))) {
    assert(!!!"Not enough data in buffer_");
  }
  return *this;
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER>& CircularBuffer<BUFFER>::operator<<(
    const std::vector<T>& v) {
  if (!v.empty()) {
    (*this) << static_cast<unsigned short>(v.size());
    for (auto it = v.begin(); it != v.end(); it++) {
      (*this) << *it;
    }
  }
}

//////////////////////////////////////////////////////////////////////////
using BUFFER = CircularBuffer<std::vector<char>>;

// 全特化版本
template <>
inline BUFFER::CircularBuffer(std::size_t maxSize)
    : maxSize_(roundUp2Power(maxSize)),
      readPos_(0),
      writePos_(0),
      buffer_(maxSize) {
  assert(0 == (maxSize_ & (maxSize_ - 1)) && "maxSize_ MUST BE power of 2");
}

template <int N>
class StackBuffer : public CircularBuffer<char[N]> {
  // 引入基类的变量以供直接使用（protected属性）
  using CircularBuffer<char[N]>::maxSize_;

 public:
  StackBuffer() {
    maxSize_ = N;
    if (maxSize_ < 0) {
      maxSize_ = 1;
    }
    if ((maxSize_ & (maxSize_ - 1)) != 0) {
      maxSize_ = roundUp2Power(maxSize_);
    }
  }
};

// 关联外部已有的内存而不是自己管理
using AttachBuffer = CircularBuffer<char*>;
template <>
inline AttachBuffer::CircularBuffer(char* pBuf, std::size_t len)
    : maxSize_(roundUp2Power(len + 1)), readPos_(0), writePos_(len) {
  buffer_ = pBuf;
  owned_ = false;
}

template <>
inline AttachBuffer::CircularBuffer(const BufferSequence& bf)
    : readPos_(0), writePos_(0) {
  owned_ = false;

  if (bf.count == 0) {
    buffer_ = 0;
  } else if (bf.count == 1) {
    buffer_ = (char*)bf.buffers[0].iov_base;
    writePos_ = static_cast<int>(bf.buffers[0].iov_len);
  } else if (bf.count > 1) {
    owned_ = true;
    buffer_ = new char[bf.totalBytes()];

    std::size_t offset = 0;
    for (std::size_t i = 0; i < bf.count; ++i) {
      ::memcpy(buffer_ + offset, bf.buffers[i].iov_base, bf.buffers[i].iov_len);
      offset += bf.buffers[i].iov_len;
    }
    writePos_ = bf.totalBytes();
  }
  maxSize_ = roundUp2Power(writePos_ - readPos_ + 1);
}

template <>
inline AttachBuffer::~CircularBuffer() {
  if (owned_) {
    delete[] buffer_;
  }
}

template <typename T>
inline void overWriteAt(void* addr, T data) {
  ::memcpy(addr, &data, sizeof(data));
}
#endif