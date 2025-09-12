#include <base/buffer/unboundedBuffer.h>
#include <spdlog/spdlog.h>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <limits>

namespace tinyredis {
const std::size_t UnboundedBuffer::MAX_BUFFER_SIZE =
    std::numeric_limits<std::size_t>::max() / 2;

std::size_t UnboundedBuffer::write(const void* pData, std::size_t nSize) {
  return pushData(pData, nSize);
}

std::size_t UnboundedBuffer::pushDataAt(const void* pData, std::size_t nSize,
                                        std::size_t offset) {
  // 边界检查
  if (!pData || nSize == 0)
    return 0;

  // 容量检查
  if (readableSize() == MAX_BUFFER_SIZE)
    return 0;

  // 确保缓冲区有足够空间容纳数据（必要时扩容）
  _AssureSpace(nSize + offset);

  assert(nSize + offset <= writableSize());
  ::memcpy(&buffer_[writePos_ + offset], pData, nSize);
  return nSize;
}

std::size_t UnboundedBuffer::pushData(const void* pData, std::size_t nSize) {
  std::size_t nBytes = pushDataAt(pData, nSize);
  adjustWritePtr(nBytes);  //  writePos_ += nBytes;
  return nBytes;
}

std::size_t UnboundedBuffer::peekDataAt(void* pBuf, std::size_t nSize,
                                        std::size_t offset) {
  const size_t dataSize = readableSize();
  // 检查。 dataSize <= offset ： 偏移值过大
  if (!pBuf || nSize == 0 || dataSize <= offset)
    return 0;

  // 修正读取长度：若请求读取的长度超过实际可用，截断为实际可读取的长度
  if (nSize + offset > dataSize)
    nSize = dataSize - offset;

  ::memcpy(pBuf, &buffer_[readPos_ + offset], nSize);
  return nSize;
}

std::size_t UnboundedBuffer::peekData(void* pBuf, std::size_t nSize) {
  std::size_t nBytes = peekDataAt(pBuf, nSize);
  
  // 将读过的数据标记为无效
  adjustReadPtr(nBytes);  // readPos_ += nBytes;
  
  return nBytes;
}

void UnboundedBuffer::_AssureSpace(std::size_t nSize) {
  if (nSize <= writableSize()) {
    return;
  }

  std::size_t maxSize = buffer_.size();

  // 循环扩容：直到有足够空间（可写空间 + 已读空间）
  while (nSize > writableSize() + readPos_) {
    // 扩容策略：小容量时直接扩到64，之后每次增加50%，不超过最大限制
    if (maxSize < 64) {
      maxSize = 64;
    } else if (maxSize <= UnboundedBuffer::MAX_BUFFER_SIZE) {
      maxSize += (maxSize / 2);
    } else
      break;
    buffer_.reserve(maxSize);
  }

  // 数据迁移：如果有已读数据（readPos_ > 0），将有效数据前移，释放前部空间
  if (readPos_ > 0) {
    std::size_t dataSize = readableSize();
    spdlog::info("{} bytes moved from {}", dataSize, readPos_);
    ::memmove(&buffer_[0], &buffer_[readPos_], dataSize);
    readPos_ = 0;
    writePos_ = dataSize;
  }
}

}  // namespace tinyredis