#include <base/buffer/asyncBuffer.h>
#include <base/buffer/buffer.h>
#include <cassert>
#include <cstddef>
#include <mutex>

AsyncBuffer::AsyncBuffer(std::size_t size) : buffer_(size), backBytes_(0) {}
AsyncBuffer::~AsyncBuffer() {}

void AsyncBuffer::write(const void* data, std::size_t len) {
  BufferSequence bf;
  bf.buffers[0].iov_base = &data;
  bf.buffers[0].iov_len = len;
  bf.count = 1;
  this->write(bf);
}

void AsyncBuffer::write(const BufferSequence& data) {
  auto len = data.totalBytes();

  // 条件1：backBytes_ > 0 ?
  // 条件2：主缓冲区可写空间不足
  if (backBytes_ > 0 || buffer_.writableSize() < len) {
    std::lock_guard<std::mutex> guard(backBufferLock_);
    // 二次检查（防止加锁前条件已变化）
    if (backBytes_ > 0 || buffer_.writableSize() < len) {
      // 将数据写入备用缓冲区
      for (std::size_t i = 0; i < data.count; ++i) {
        backBuffer_.pushData(data.buffers[i].iov_base, data.buffers[i].iov_len);
      }
      backBytes_ += len;  // 更新备用缓冲区的数据量（原子操作）
      assert(backBytes_ == backBuffer_.readableSize());  // 验证数据量一致性
      return;
    }
    assert(backBytes_ == 0 && buffer_.writableSize() >= len);
    for (size_t i = 0; i < data.count; ++i) {
      buffer_.pushData(data.buffers[i].iov_base, data.buffers[i].iov_len);
    }
  }
}

// 将缓冲区中的数据提取到 BufferSequence 结构
void AsyncBuffer::processBuffer(BufferSequence& data) {
  data.count = 0;

  // 临时缓冲区tmpBuf_中有数据（上次从backBuf_迁移的剩余数据）
  if (!tmpBuffer_.isEmpty()) {
    data.count = 1;
    data.buffers[0].iov_base = tmpBuffer_.readAddr();
    data.buffers[0].iov_len = tmpBuffer_.readableSize();
  } else if (!buffer_.isEmpty()) {  // 主缓冲区buffer_中有数据
    auto nLen = buffer_.readableSize();
    buffer_.getDatum(data, nLen);
    assert(nLen == data.totalBytes());
  } else {  // 主缓冲和临时缓冲都为空，尝试处理备用缓冲backBuf_的数据
    if (backBytes_ > 0 && backBufferLock_.try_lock()) {
      backBytes_ = 0;
      tmpBuffer_.swap(backBuffer_);
      backBufferLock_.unlock();

      data.count = 1;
      data.buffers[0].iov_base = tmpBuffer_.readAddr();
      data.buffers[0].iov_len = tmpBuffer_.readableSize();
    }
  }
}

// 跳过缓冲区中指定大小的数据（标记为 “已处理”）
void AsyncBuffer::skip(std::size_t size) {
  // 临时缓冲区tmpBuf_中有数据
  if (!tmpBuffer_.isEmpty()) {
    assert(size <= tmpBuffer_.readableSize());
    tmpBuffer_.adjustReadPtr(size);
  } else {
    // 临时缓冲区为空，则操作主缓冲区buffer_
    assert(buffer_.readableSize() >= size);
    buffer_.adjustReadAddr(size);
  }
}