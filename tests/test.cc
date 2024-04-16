#include <iostream>
#include "sylar/logger.h"
using namespace std;

int main(){
    sylar::Logger::ptr logger(new sylar::Logger("test"));
    logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender));

    sylar::LogEvent::ptr event(new sylar::LogEvent(__FILE__, __LINE__, 0, 1, 2, time(0)));
    logger->log(sylar::LogLevel::DEBUG, event);
    return 0;
}