/*完成日志器模块
	1抽象日志器基类
	2派生出不同子类(同步日志器和异步日志器)
*/
#pragma once
#include "util.hpp"
#include "level.hpp"
#include "format.hpp"
#include "sink.hpp"
#include <atomic>
#include <mutex>
#include <vector>

namespace my_log {
	class Logger {
	public:
		using ptr = std::shared_ptr<Logger>;
		/*完成构造日志消息对象过程并进行初始化，得到格式化后的日志消息字符串，然后进行落地输出*/
		void debug(const string& file, size_t line, const std::string& fmt, ...);
		void info(const string& file, size_t line, const std::string& fmt, ...);
		void warn(const string& file, size_t line, const std::string& fmt, ...);
		void error(const string& file, size_t line, const std::string& fmt, ...);
		void fatal(const string& file, size_t line, const std::string& fmt, ...);
	private:
		virtual void log(const char* data, size_ len);
	private:
		std::string _logger_name;
		std::atomic<LogLevel::value> _limit_level;
		FormatItem::ptr _formatter;
		std::vector<LogSink::ptr> _sinks;
	};

	class SynLogger : public Logger {
	protected:
		void log(const char* data, size_t len);
	};
}