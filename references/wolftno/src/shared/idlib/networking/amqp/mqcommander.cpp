#include "shared/idlib/networking/amqp/mq.h"
idMQCommandBuilder& idMQCommandBuilder::Name(const std::string& n) {
  command.name = n;
  return *this;
}
idMQCommandBuilder& idMQCommandBuilder::Arg(const std::string& a) {
  command.arguments.push_back(a);
  return *this;
}
