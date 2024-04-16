#include "logger.h"

namespace sylar {

const char* LogLevelToString(LogLevel level) {
    switch(static_cast<int>(level)) {
        case static_cast<int>(LogLevel::DEBUG):
            return "DEBUG";
        case static_cast<int>(LogLevel::INFO):
            return "INFO";
        case static_cast<int>(LogLevel::WARN):
            return "WARN";
        case static_cast<int>(LogLevel::ERROR):
            return "ERROR";
        case static_cast<int>(LogLevel::FATAL):
            return "FATAL";
        default:
            return "UNKNOW";
    }
    return "UNKNOW";
} 

LogEvent::LogEvent(/*std::shared_ptr<Logger> logger, LogLevel level,*/ const char* file, int32_t line, uint32_t elapse, uint32_t thread_id, uint32_t fiber_id, uint64_t time)
    :m_file(file),
    m_line(line),
    m_elapse(elapse),
    m_threadId(thread_id),
    m_fiberId(fiber_id),
    m_time(time) //,
    // m_logger(logger),
    // m_level(level)
    {
}

Logger::Logger(const std::string& name)
    :m_name(name),
    m_level(LogLevel::DEBUG){
        m_formatter.reset(new LogFormatter("(%d) [%p] <%f:%l>\t%m %n"));
}

void Logger::log(LogLevel level, LogEvent::ptr event) {
    if(level >= m_level) {
        auto self = shared_from_this();
        for(auto& appender : m_appenders) {
            appender->log(self, level, event);
        }
    }
}

void Logger::debug(LogEvent::ptr event) {
    log(LogLevel::DEBUG, event);
}

void Logger::info(LogEvent::ptr event) {
    log(LogLevel::INFO, event);
}

void Logger::warn(LogEvent::ptr event) {
    log(LogLevel::WARN, event);
}

void Logger::error(LogEvent::ptr event) {
    log(LogLevel::ERROR, event);
}

void Logger::fatal(LogEvent::ptr event) {
    log(LogLevel::FATAL, event);
}

void Logger::addAppender(LogAppender::ptr appender) {
    if(!appender->getFormatter()) {
        appender->setFormatter(m_formatter);
    }
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
    for(auto it = m_appenders.begin(); it != m_appenders.end(); ++it) {
        if(*it == appender) {
            m_appenders.erase(it);
            break;
        }
    }
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel level, LogEvent::ptr event) {
    if(level >= m_level) {
        std::cout << m_formatter->format(logger, level, event);
    }
}

FileLogAppender::FileLogAppender(const std::string& filename)
    :m_filename(filename) {
}

void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel level, LogEvent::ptr event) {
    if(level >= m_level) {
        m_filestream << m_formatter->format(logger, level, event);
    }
}

bool FileLogAppender::reopen() {
    if(m_filestream) {
        m_filestream.close();
    }
    m_filestream.open(m_filename);
    return !!m_filestream; // !! is used to convert the result to a boolean value
}

LogFormatter::LogFormatter(const std::string& pattern)
    :m_pattern(pattern) {
    init();
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel level, LogEvent::ptr event) {
    std::stringstream ss;
    for(auto& i : m_items) {
        i->format(ss, logger, level, event);
    }
    return ss.str();
}

void LogFormatter::init() { // 日志格式解析
    // 格式类似于: %xxx %xxx{xxx} %%
    // str, format, type
    std::vector<std::tuple<std::string, std::string, int>> vec;
    std::string nstr; // 用于保存非转义字符
    for(size_t i = 0; i < m_pattern.size(); ++i) {
        if(m_pattern[i] != '%') {
            nstr.append(1, m_pattern[i]); // append是在字符串后面添加字符
            continue;
        }

        // 转义字符
        if((i + 1) < m_pattern.size()) {
            if(m_pattern[i + 1] == '%') {
                nstr.append(1, '%'); 
                ++i;
                continue;
            }
        }

        size_t n = i + 1;  // n是i的下一个位置
        int fmt_status = 0; // 0: 初始状态, 1: 进入格式状态
        size_t fmt_begin = 0; // 格式开始位置

        std::string str; // 占位符
        std::string fmt; // 格式
        while(n < m_pattern.size()) {
            // isalpha()函数用来判断一个字符是否是字母
            if(!fmt_status && (!isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}')) {
                str = m_pattern.substr(i + 1, n - i - 1);
                break;
            }
            if(fmt_status == 0) {
                if(m_pattern[n] == '{') {
                    str = m_pattern.substr(i + 1, n - i - 1);
                    fmt_status = 1; // enter the format mode
                    fmt_begin = n;
                    ++n;
                    continue;
                }
            } else if(fmt_status == 1) {
                if(m_pattern[n] == '}') {
                    fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                    fmt_status = 0;
                    ++n;
                    break;
                }
            }
            ++n;
            if(n == m_pattern.size()) {
                if(str.empty()) {
                    str = m_pattern.substr(i + 1);
                }
            }
        }

        if(fmt_status == 0) {
            if(!nstr.empty()) {
                vec.push_back(std::make_tuple(nstr, std::string(), 0));
                nstr.clear();
            }
            vec.push_back(std::make_tuple(str, fmt, 1));
            i = n - 1;
        } else if(fmt_status == 1) {
            std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
        }
    }

    // 使用直接初始化的方式构造 map
    static std::map<std::string, std::function<FormatItem::ptr(const std::string&)>> s_format_items = {
        {"m", [](const std::string& fmt) { return std::make_shared<MessageFormatItem>(fmt); }},         // %m -- 消息体
        {"p", [](const std::string& fmt) { return std::make_shared<LevelFormatItem>(fmt); }},           // %p -- level
        {"r", [](const std::string& fmt) { return std::make_shared<ElapseFormatItem>(fmt); }},          // %r -- 启动后的时间
        {"c", [](const std::string& fmt) { return std::make_shared<NameFormatItem>(fmt); }},            // %c -- 日志名称
        {"t", [](const std::string& fmt) { return std::make_shared<ThreadIdFormatItem>(fmt); }},        // %t -- 线程id
        {"n", [](const std::string& fmt) { return std::make_shared<NewLineFormatItem>(fmt); }},         // %n -- 回车换行
        {"d", [](const std::string& fmt) { return std::make_shared<DateTimeFormatItem>(fmt); }},        // %d -- 时间
        {"f", [](const std::string& fmt) { return std::make_shared<FilenameFormatItem>(fmt); }},        // %f -- 文件名
        {"l", [](const std::string& fmt) { return std::make_shared<LineFormatItem>(fmt); }}             // %l -- 行号
    };


    for (auto& i : vec) {
        if(std::get<2>(i) == 0) {
            m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i)))); // get<0>(i) 意味着取出 tuple 元组i的第一个元素
        } else {
            auto it = s_format_items.find(std::get<0>(i));
            if(it == s_format_items.end()) {
                m_items.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
            } else {
                m_items.push_back(it->second(std::get<1>(i)));
            }
        }
    }
}






}