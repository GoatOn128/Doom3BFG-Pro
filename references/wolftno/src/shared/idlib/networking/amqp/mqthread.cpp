#include "shared/idlib/networking/amqp/mq.h"
idMQClientThread::idMQClientThread() {
  StartWorkerThread("idMQClientThread");
}
idMQClientThread::~idMQClientThread() {
  StopThread(true);
}
void idMQClientThread::Enqueue(idMQCommand c) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    commands.push_back(std::move(c));
  }
  SignalWork();
}
bool idMQClientThread::TryDequeue(idMQCommand& c) {
  std::lock_guard<std::mutex> lock(mutex);
  if (commands.empty())
    return false;
  c = std::move(commands.front());
  commands.pop_front();
  return true;
}
int idMQClientThread::Run() {
  int count = 0;
  idMQCommand command;
  while (TryDequeue(command))
    ++count;
  return count;
}
