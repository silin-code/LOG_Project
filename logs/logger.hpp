/*完成日志器模块
    1抽象日志器基类
    2派生出不同子类(同步日志器和异步日志器)
    3统一调度：等级过滤->格式化->日志落地
    核心流程：调用日志接口 → 等级判断 → 格式化字符串 → 构造日志消息 → 输出到落地器
*/
#pragma once
#include <iostream>
#include <string>
#include <atomic>
#include <mutex>
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <memory>
#include <unordered_map>
#include <cassert>

#include "util.hpp"
#include "level.hpp"
#include "format.hpp"
#include "message.hpp"
#include "sink.hpp"
#include "looper.hpp"


namespace my_log {
    class LoggerManager;

    class Logger {
    public:
        using ptr = std::shared_ptr<Logger>;
        /*
        日志器构造函数（核心初始化）
        参数：
           logger_name：日志器名称
           level：日志输出等级阈值
           formatter：格式化器智能指针
           sinks：落地器数组
        */
        Logger(const std::string& logger_name, LogLevel::value level,
            const Formatter::ptr& formatter, const std::vector<LogSink::ptr>& sinks)
            : _logger_name(logger_name)
            , _limit_level(level)
            , _formatter(formatter)
            , _sinks(sinks.begin(), sinks.end())
        {
        }

        virtual ~Logger() = default;

        const std::string& name() { return _logger_name; }

        // 对外接口
        void debug(const std::string& file, size_t line, const char* fmt, ...);
        void info(const std::string& file, size_t line, const char* fmt, ...);
        void warn(const std::string& file, size_t line, const char* fmt, ...);
        void error(const std::string& file, size_t line, const char* fmt, ...);
        void fatal(const std::string& file, size_t line, const char* fmt, ...);

    protected:
        virtual void log(const char* data, size_t len) = 0;

        void serialize(LogLevel::value level, const std::string& file, size_t line, const char* str) {
            LogMsg msg(level, line, file, _logger_name, str);
            std::stringstream ss;
            _formatter->format(ss, msg);
            log(ss.str().c_str(), ss.str().size());
        }

        std::string formatString(const std::string& fmt, va_list ap) {
            va_list ap_copy;
            va_copy(ap_copy, ap);
            int len = vsnprintf(nullptr, 0, fmt.c_str(), ap_copy);
            va_end(ap_copy);

            std::string result(len + 1, '\0');
            vsnprintf(&result[0], len + 1, fmt.c_str(), ap);
            return result;
        }

    protected:
        SpinLock _mutex;
        std::string _logger_name;
        std::atomic<LogLevel::value> _limit_level;
        Formatter::ptr _formatter;
        std::vector<LogSink::ptr> _sinks;
    };

    // 同步日志器
    class SyncLogger : public Logger {
    public:
        using ptr = std::shared_ptr<SyncLogger>;

        SyncLogger(const std::string& logger_name, LogLevel::value level,
            const Formatter::ptr formatter, const std::vector<LogSink::ptr>& sinks)
            : Logger(logger_name, level, formatter, sinks) {
        }

    protected:
        void log(const char* data, size_t len) override {
            SpinLockGuard lock(_mutex);
            if (_sinks.empty()) return;
            for (auto& sink : _sinks) {
                sink->log(data, len);
            }
        }
    };

    // 异步日志器
    class AsyncLogger : public Logger {
    public:
        AsyncLogger(const std::string& logger_name, LogLevel::value level,
            const Formatter::ptr formatter, const std::vector<LogSink::ptr>& sinks,
            AsyncType looper_type)
            : Logger(logger_name, level, formatter, sinks)
            , _looper(std::make_shared<AsyncLooper>(
                std::bind(&AsyncLogger::readlog, this, std::placeholders::_1),
                looper_type))
        {
        }

        void log(const char* data, size_t len) override {
            _looper->push(data, len);
        }

        void readlog(Buffer& buf) {
            if (_sinks.empty()) return;
            for (auto& sink : _sinks) {
                sink->log(buf.begin(), buf.readAbleSize());
            }
        }

    private:
        AsyncLooper::ptr _looper;
    };

    // ==================== 接口实现 ====================
    inline void Logger::debug(const std::string& file, size_t line, const char* fmt, ...) {
        if (LogLevel::value::DEBUG < _limit_level.load()) return;
        va_list ap;
        va_start(ap, fmt);
        std::string str = formatString(fmt, ap);
        va_end(ap);
        serialize(LogLevel::value::DEBUG, file, line, str.c_str());
    }
    inline void Logger::info(const std::string& file, size_t line, const char* fmt, ...) {
        if (LogLevel::value::INFO < _limit_level.load()) return;
        va_list ap;
        va_start(ap, fmt);
        std::string str = formatString(fmt, ap);
        va_end(ap);
        serialize(LogLevel::value::INFO, file, line, str.c_str());
    }
    inline void Logger::warn(const std::string& file, size_t line, const char* fmt, ...) {
        if (LogLevel::value::WARN < _limit_level.load()) return;
        va_list ap;
        va_start(ap, fmt);
        std::string str = formatString(fmt, ap);
        va_end(ap);
        serialize(LogLevel::value::WARN, file, line, str.c_str());
    }
    inline void Logger::error(const std::string& file, size_t line, const char* fmt, ...) {
        if (LogLevel::value::ERR < _limit_level.load()) return;
        va_list ap;
        va_start(ap, fmt);
        std::string str = formatString(fmt, ap);
        va_end(ap);
        serialize(LogLevel::value::ERR, file, line, str.c_str());
    }
    inline void Logger::fatal(const std::string& file, size_t line, const char* fmt, ...) {
        if (LogLevel::value::FATAL < _limit_level.load()) return;
        va_list ap;
        va_start(ap, fmt);
        std::string str = formatString(fmt, ap);
        va_end(ap);
        serialize(LogLevel::value::FATAL, file, line, str.c_str());
    }

    enum class LoggerType {
        LOGGER_SYNC,
        LOGGER_ASYNC
    };

    // 建造者模式
    class LoggerBuilder {
    public:
        LoggerBuilder()
            : _logger_type(LoggerType::LOGGER_SYNC)
            , _limit_level(LogLevel::value::DEBUG)
            , _looper_type(AsyncType::ASYNC_SAFE)
        {
        }

        void buildLoggerType(LoggerType type) { _logger_type = type; }
        void buildEnableUnsafe() { _looper_type = AsyncType::ASYNC_UNSAFE; }
        void buildLoggerName(const std::string& name) { _logger_name = name; }
        void buildLoggerLevel(LogLevel::value level) { _limit_level = level; }
        void buildFormatter(const std::string& pattern) {
            _formatter = std::make_shared<Formatter>(pattern);
        }

        template<typename SinkType, typename ...Args>
        void buildSink(Args &&...args) {
            LogSink::ptr psink = SinkFactory::create<SinkType>(std::forward<Args>(args)...);
            _sinks.push_back(psink);
        }

        virtual Logger::ptr build() = 0;

    protected:
        AsyncType _looper_type;
        LoggerType _logger_type;
        std::string _logger_name;
        LogLevel::value _limit_level;
        Formatter::ptr _formatter;
        std::vector<LogSink::ptr> _sinks;
    };

    // 局部建造者（不再自动将日志器加入全局管理器）
    class LocalLoggerBuilder : public LoggerBuilder {
    public:
        Logger::ptr build() override;
    };

    // 日志器管理器（单例）
    class LoggerManager {
    public:
        static LoggerManager& getInstance() {
            static LoggerManager eton;
            return eton;
        }

        void addLogger(const Logger::ptr& logger) {
            std::unique_lock<std::mutex> lock(_mutex);
            // 检查+插入在同一个锁内，避免竞态
            if (_loggers.find(logger->name()) != _loggers.end()) return;
            _loggers[logger->name()] = logger;
        }

        bool hasLogger(const std::string& name) {
            std::unique_lock<std::mutex> lock(_mutex);
            return _loggers.find(name) != _loggers.end();
        }

        Logger::ptr getLogger(const std::string& name) {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _loggers.find(name);
            if (it == _loggers.end()) return Logger::ptr();
            return it->second;
        }

        Logger::ptr rootLogger() {
            return _root_logger;
        }

    private:
        LoggerManager() {
            // 手动构建root日志器，并在构造期间直接加入（不再调用 getInstance()）
            std::unique_ptr<my_log::LoggerBuilder> builder(new my_log::LocalLoggerBuilder());
            builder->buildLoggerName("root");
            _root_logger = builder->build();
            // 直接操作成员，此时对象已构造完成（成员已初始化）
            _loggers["root"] = _root_logger;
        }

    private:
        std::mutex _mutex;
        Logger::ptr _root_logger;
        std::unordered_map<std::string, Logger::ptr> _loggers;
    };

    inline Logger::ptr LocalLoggerBuilder::build() {
        assert(!_logger_name.empty());
        if (_formatter.get() == nullptr) {
            _formatter = std::make_shared<Formatter>();
        }
        if (_sinks.empty()) {
            buildSink<StdoutSink>();
        }

        Logger::ptr logger;
        if (_logger_type == LoggerType::LOGGER_ASYNC) {
            logger = std::make_shared<AsyncLogger>(_logger_name, _limit_level, _formatter,
                _sinks, _looper_type);
        }
        else {
            logger = std::make_shared<SyncLogger>(_logger_name, _limit_level, _formatter, _sinks);
        }
        // 不再自动加入全局管理器，由调用者决定是否注册
        return logger;
    }
} // namespace my_log