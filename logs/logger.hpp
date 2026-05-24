/*完成日志器模块
	1抽象日志器基类
	2派生出不同子类(同步日志器和异步日志器)
	3统一调度：等级过滤->格式化->日志落地
	核心流程：调用日志接口 → 等级判断 → 格式化字符串 → 构造日志消息 → 输出到落地器
*/
#pragma once
#include <iostream>
#include <string>
#include <atomic>//原子类型，线程安全日志等级
#include <mutex>
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <memory>

#include "util.hpp"
#include "level.hpp"
#include "format.hpp"
#include "message.hpp"
#include "sink.hpp"
#include "looper.hpp"


namespace my_log {
	class Logger {
	public:
		using ptr = std::shared_ptr<Logger>;
		/*
		日志器构造函数（核心初始化）
	参数：
	   logger_name：日志器名称（区分不同日志器，如 root、user、db）
	   level：日志输出等级（低于该等级的日志直接丢弃）
	   formatter：格式化器智能指针（负责把日志消息变成指定格式）
	   sinks：落地器数组（一个日志器可以同时输出到控制台+文件+滚动文件）
   注意：这里用 & 引用传递，避免拷贝智能指针/vector，提升效率
		*/
		Logger(const std::string& logger_name,LogLevel::value level,const Formatter::ptr &formatter,const std::vector<LogSink::ptr> &sinks):
			_logger_name(logger_name),
			_limit_level(level),
			_formatter(formatter),
			_sinks(sinks.begin(),sinks.end()){ }
		
		virtual ~Logger() = default; // 虚析构函数：子类析构时能正确调用基类析构，防止内存泄漏

		/*完成构造日志消息对象过程并进行初始化，得到格式化后的日志消息字符串，然后进行落地输出*/
		// ===================== 对外日志接口（5个等级） =====================
		// 用户调用这些接口输出日志，参数：文件名、行号、格式化字符串、可变参数
		void debug(const std::string& file, size_t line, const char* fmt, ...);
		void info(const std::string& file, size_t line, const char* fmt, ...);
		void warn(const std::string& file, size_t line, const char* fmt, ...);
		void error(const std::string& file, size_t line, const char* fmt, ...);
		void fatal(const std::string& file, size_t line, const char* fmt, ...);

	protected:
		/*
		纯虚函数：日志落地接口
		作用：基类只定义接口，具体输出逻辑由子类实现（同步/异步）
		必须被子类重写，否则无法实例化
		参数：data-日志字符串首地址，len-字符串长度
		*/
		virtual void log(const char* data, size_t len) = 0;
	protected:
		//日志格式化与序列化
		/*
		核心函数：日志序列化（调度核心）
	作用：
		1. 构造完整的日志消息对象
		2. 调用格式化器，把日志消息变成指定格式的字符串
		3. 调用落地接口，输出日志
	参数：
		level：日志等级
		file：输出日志的源代码文件名
		line：输出日志的代码行号
		str：格式化后的日志正文（用户输入的内容）
		*/
		void serialize(LogLevel::value level, const std::string& file, size_t line,const char* str)
		{
			// 步骤1：构造日志消息对象（封装所有日志信息）
			LogMsg msg(level, line, file, _logger_name, str);

			// 步骤2：格式化日志消息
			// 字符串流：临时存储格式化后的日志字符串
			std::stringstream ss;
			// 调用格式化器：把 LogMsg 里的所有信息，按格式拼接成字符串
			_formatter->format(ss, msg);
			//步骤3：日志落地输出
			// 调用子类实现的 log 方法，输出到控制台/文件等
			log(ss.str().c_str(), ss.str().size());
		}
		//不定参格式化
		/*
	核心函数：不定参格式化
	作用：处理 printf 风格的格式化字符串（如 "日志：%d %s"）把可变参数（...）和格式化字符串拼接成完整的日志正文
	原理：用标准C函数 vsnprintf 跨平台处理格式化，全平台通用
	参数：
		fmt：格式化字符串（如 "用户%d登录成功"）
		ap：可变参数列表（存储传入的 %d %s 对应的参数）
	返回：拼接完成的日志正文
	*/
		std::string formatString(const std::string& fmt, va_list ap)
		{
			// 复制一份可变参数，防止第一次计算长度时破坏原参数
			va_list ap_copy;
			va_copy(ap_copy, ap);

			//第一步：计算格式化后的字符串长度
			//标准跨平台格式化，代替非标准vasprint
			// vsnprintf(nullptr,0,...)：不实际输出，只返回需要的字符串长度
			int len = vsnprintf(nullptr, 0, fmt.c_str(), ap_copy);
			va_end(ap_copy); // 关闭复制的参数列表


			//第二步：申请空间，拼接完整字符串
			// 创建足够大的字符串存储结果
			std::string result(len + 1, '\0');

			// 正式格式化：把参数填入格式化字符串，得到完整日志正文
			vsnprintf(&result[0], len + 1, fmt.c_str(), ap);

			//返回拼接好的日志正文
			return result;
		}
	protected:
		std::mutex _mutex;
		std::string _logger_name;
		std::atomic<LogLevel::value> _limit_level;
		Formatter::ptr _formatter;
		std::vector<LogSink::ptr> _sinks;
	};

	// 作用：同步输出日志——调用日志接口时，立即格式化+落地（阻塞式，简单稳定）
	class SyncLogger : public Logger {
	public:
		using ptr = std::shared_ptr<SyncLogger>;

		/*
   同步日志器构造函数
   直接调用父类（Logger）的构造函数，完成初始化
   参数和父类完全一致
   */
		SyncLogger(const std::string &logger_name,LogLevel::value level,const Formatter::ptr formatter,const std::vector<LogSink::ptr> &sinks):
			Logger(logger_name,level,formatter,sinks){ }

	protected:
		/*
		重写父类纯虚函数：同步日志落地实现
		作用：遍历所有落地器，把日志输出到每一个落地器（控制台/文件等）
		线程安全：加锁保护，多线程不冲突
		*/
		void log(const char* data, size_t len) override
		{
			// 加锁：同一时间只有一个线程能执行日志输出
			std::unique_lock<std::mutex> lock(_mutex);
			if (_sinks.empty())return;

			// 遍历所有落地器
			for (auto& sink : _sinks)
			{
				// 调用落地器的log方法，输出日志
				sink->log(data, len);
			}
		}
	};
	//异步日志器
	class AsyncLogger : public Logger
	{
	public:
		AsyncLogger(const std::string& logger_name, LogLevel::value level, const Formatter::ptr formatter, const std::vector<LogSink::ptr>& sinks,AsyncType looper_type) :
			Logger(logger_name, level, formatter, sinks),
			_looper(std::make_shared<AsyncLooper>(std::bind(&AsyncLogger::readlog,this,std::placeholders::_1),looper_type)){ }

		//将数据写入缓冲区
		void log(const char* data, size_t len)
		{
			_looper->push(data, len);
		}
		//设计一个实际落地函数(将缓冲区的数据落地)
		void readlog(Buffer& buf)
		{
			if (_sinks.empty()) return;
			for (auto& sink : _sinks)
			{
				sink->log(buf.begin(), buf.readAbleSize());
			}
		}

	private:
		AsyncLooper::ptr _looper;
	};

	//接口实现
	inline void Logger::debug(const std::string& file, size_t line, const char* fmt, ...)
	{
		// 等级过滤：如果当前日志等级 低于 设定的过滤等级，直接丢弃
		if (LogLevel::value::DEBUG < _limit_level)return;

		// ========== 处理可变参数 ==========
		va_list ap;             // 定义可变参数列表
		va_start(ap, fmt);      // 初始化参数列表，绑定到最后一个固定参数fmt

		// 调用格式化函数，拼接字符串
		std::string str = formatString(fmt, ap);
		va_end(ap);             // 关闭参数列表

		// 调用核心序列化函数，输出日志
		serialize(LogLevel::value::DEBUG, file, line, str.c_str());
	}

	inline void Logger::info(const std::string& file, size_t line, const char* fmt, ...) {
		if (LogLevel::value::INFO < _limit_level) return;
		va_list ap;
		va_start(ap, fmt);
		std::string str = formatString(fmt, ap);
		va_end(ap);
		serialize(LogLevel::value::INFO, file, line, str.c_str());
	}

	inline void Logger::warn(const std::string& file, size_t line, const char* fmt, ...) {
		if (LogLevel::value::WARN < _limit_level) return;
		va_list ap;
		va_start(ap, fmt);
		std::string str = formatString(fmt, ap);
		va_end(ap);
		serialize(LogLevel::value::WARN, file, line, str.c_str());
	}

	inline void Logger::error(const std::string& file, size_t line, const char* fmt, ...) {
		if (LogLevel::value::ERR < _limit_level) return;
		va_list ap;
		va_start(ap, fmt);
		std::string str = formatString(fmt, ap);
		va_end(ap);
		serialize(LogLevel::value::ERR, file, line, str.c_str());
	}

	inline void Logger::fatal(const std::string& file, size_t line, const char* fmt, ...) {
		if (LogLevel::value::FATAL < _limit_level) return;
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

	/*使用建造者模式来建造日志器，而不要让用户直接去构造日志器，简化用户的使用复杂度*/
	//1：抽象一个日志器建造者类（完成日志器对象所需的零部件的构造和日志器的构建）
	//	1：设置日志器类型
	//	2：将不同类型日志器的创建放到同一个日志器建造者类中完成
	class LoggerBuilder {
	public:
		LoggerBuilder():
			_logger_type(LoggerType::LOGGER_SYNC),
			_limit_level(LogLevel::value::DEBUG),
			_looper_type(AsyncType::ASYNC_SAFE)
		{ }

		void buildLoggerType(LoggerType type) { _logger_type = type; }
		void buildEnableUnsafe() { _looper_type = AsyncType::ASYNC_UNSAFE; }
		void buildLoggerName(const std::string& name) { _logger_name = name; }
		void buildLoggerLevel(LogLevel::value level) { _limit_level = level; }
		void buildFormatter(const std::string& pattern) {
			_formatter = std::make_shared<Formatter>(pattern);
		}

		template<typename SinkType,typename ...Args>
		void buildSink(Args &&...args)
		{
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

	//2：派生出具体的建造者类--局部日志器的建造者 和 全局的日志器建造者(后边添加了全局单例管理器之后，将日志器添加到全局管理)
	class LocalLoggerBuilder :public LoggerBuilder {
	public:
		Logger::ptr build() override
		{
			assert(!_logger_name.empty());//必须有日志器名称
			if (_formatter.get() == nullptr)
			{
				_formatter = std::make_shared<Formatter>();
			}
			if (_sinks.empty())
			{
				buildSink<StdoutSink>();
			}

			if (_logger_type == LoggerType::LOGGER_ASYNC)
			{
				return std::make_shared<AsyncLogger>(_logger_name, _limit_level, _formatter, _sinks,_looper_type);
			}
			return std::make_shared<SyncLogger>(_logger_name, _limit_level, _formatter, _sinks);
		}
	};
}//namespace my_log