#include <base/taskManager.h>
#include <cassert>
#include <map>
#include <mutex>
#include "spdlog/spdlog.h"

namespace Internal {
TaskManager::~TaskManager() {
  assert(empty() && "Why you do not clear container before exit?");
}

bool TaskManager::addTask(PTCPSOCKET task) {
  std::lock_guard<std::mutex> guard(lock_);
  newTasks_.push_back(task);
  ++newCnt_;  // 新任务数量
  return true;
}

TaskManager::PTCPSOCKET TaskManager::findTCP(unsigned int id) const {
  if (id > 0) {
    auto it = tcpSockets_.find(id);
    if (it != tcpSockets_.end())
      return it->second;
  }
  return PTCPSOCKET();
}

bool TaskManager::_AddTask(PTCPSOCKET task) {
  bool success = tcpSockets_.insert({task->getID(), task}).second;
  return success;
}

void TaskManager::_RemoveTask(std::map<int, PTCPSOCKET>::iterator& it) {
  tcpSockets_.erase(it++);  // 采用 it++ 确保在删除后迭代器仍然有效
}

bool TaskManager::DoMsgParse() {
  if (newCnt_ > 0 && lock_.try_lock()) {
    NEWTASK_T tmpNewTasks;
    tmpNewTasks.swap(newTasks_);
    newCnt_ = 0;
    lock_.unlock();

    for (const auto& task : newTasks_) {
      if (!_AddTask(task)) {
        spdlog::error("Why can not insert tcp socket {} , id = {}",
                      task->getSocket(), task->getID());
      } else {
        spdlog::info("New connection from {}, id = {}",
                     task->getPeerAddr().toString(), task->getID());
        // 调用任务的连接成功回调
        task->OnConnect();
      }
    }
  }
  bool busy = false;  // 标记是否有任务在处理消息
  for (auto it(tcpSockets_.begin()); it != tcpSockets_.end();) {
    // 检查任务是否无效（智能指针为空或套接字无效）
    if (it->second || it->second->invalid()) {
      if (it->second) {
        spdlog::info("Close connection from {}, id = {}",
                     it->second->getPeerAddr().toString(), it->second->getID());
        it->second->OnDisconnect();
      }
      _RemoveTask(it);  // 会it++
    } else {
      // 调用任务自身的消息解析方法
      // 如果有任务在处理消息且当前busy为false，则标记为busy
      if (it->second->DoMsgParse() && !busy) {
        busy = true;
      }
      ++it;
    }
  }
  // 返回是否有任务在处理消息
  return busy;
}
}  // namespace Internal