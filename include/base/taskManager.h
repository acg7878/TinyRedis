#ifndef BASE_TASKMANAGER_H
#define BASE_TASKMANAGER_H

#include <base/socket/streamSocket.h>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace Internal {
class TaskManager {
  using PTCPSOCKET = std::shared_ptr<StreamSocket>;
  using NEWTASK_T = std::vector<PTCPSOCKET>;

 public:
  TaskManager() : newCnt_(0) {}
  ~TaskManager();

  bool addTask(PTCPSOCKET task);
  bool empty() { return tcpSockets_.empty(); }
  void clear() { tcpSockets_.clear(); }
  std::size_t size() const { return tcpSockets_.size(); }
  PTCPSOCKET findTCP(unsigned int id) const;
  bool DoMsgParse();

 private:
  bool _AddTask(PTCPSOCKET task);
  void _RemoveTask(std::map<int, PTCPSOCKET>::iterator&);
  std::map<int, PTCPSOCKET> tcpSockets_;  // 正式存储所有活跃任务

  std::mutex lock_;
  NEWTASK_T newTasks_;       // 临时存储新添加的任务
  std::atomic<int> newCnt_;  // vector::empty() 并非线程安全
};
}  // namespace Internal

#endif
