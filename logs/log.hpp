#pragma once
/*
1提供获取指定日志器的全局接口（避免用户自己操作单例对象）
2使用宏函数对日志器的接口进行代理（代理模式）
3提供宏函数，直接通过默认日志器进行日志的标准输出打印（不用获取日志器）
*/
#include "logger.hpp"

namespace my_log {
	//1提供获取指定日志器的全局接口（避免用户自己操作单例对象）
	Logger::ptr getLogger(const std::string& name)
	{
		return my_log::LoggerManager::getInstance().getLogger("name");
	}

	Logger::ptr rootLogger()
	{
		return my_log::LoggerManager::getInstance().rootLogger();
	}

	//2使用宏函数对日志器的接口进行代理（代理模式）
	//使用C++可变参数模板代替宏
	//作用：给指定日志器打印日志，自动填充文件名+行号，类型安全
	template<typename...Args>
	inline void LOG_DEBUG(const Logger::ptr& logger, const char* fmt, Args&&... args)
	{
		if (!logger||!fmt)return;
		logger->debug(__FILE__, __LINE__, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void LOG_INFO(const Logger::ptr& logger, const char* fmt, Args&&... args)
	{
		if (!logger || !fmt) return;
		logger->info(__FILE__, __LINE__, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void LOG_WARN(const Logger::ptr& logger, const char* fmt, Args&&... args)
	{
		if (!logger || !fmt) return;
		logger->warn(__FILE__, __LINE__, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void LOG_ERR(const Logger::ptr& logger, const char* fmt, Args&&... args)
	{
		if (!logger || !fmt) return;
		logger->error(__FILE__, __LINE__, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void LOG_FATAL(const Logger::ptr& logger, const char* fmt, Args&&... args)
	{
		if (!logger || !fmt) return;
		logger->fatal(__FILE__, __LINE__, fmt, std::forward<Args>(args)...);
	}

	//3默认根据日治其快捷接口（不用手动获取logger）
	template<typename... Args>
	inline void DEBUG(const char* fmt, Args&&...args)
	{
		LOG_DEBUG(rootLogger(), fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void INFO(const char* fmt, Args&&... args)
	{
		LOG_INFO(rootLogger(), fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void WARN(const char* fmt, Args&&... args)
	{
		LOG_WARN(rootLogger(), fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void ERR(const char* fmt, Args&&... args)
	{
		LOG_ERR(rootLogger(), fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	inline void FATAL(const char* fmt, Args&&... args)
	{
		LOG_FATAL(rootLogger(), fmt, std::forward<Args>(args)...);
	}
}