/*
1定义枚举类，枚举出日志等级
2定义转换接口，将枚举类转换成对应字符串
*/
#pragma once

namespace my_log {
	class LogLevel {
	public:
		enum class value {
			UNKNOW=0,
			DEBUG,
			INFO,
			WARN,
			ERR,
			FATAL,
			OFF
		};
		static const char* ToString(LogLevel::value level)
		{
			switch (level){
			case LogLevel::value::DEBUG: return "DEBUG";
			case LogLevel::value::INFO: return "INFO";
			case LogLevel::value::WARN: return "WARN";
			case LogLevel::value::ERR: return "ERR";
			case LogLevel::value::FATAL: return "FATAL";
			case LogLevel::value::OFF: return "OFF";
			}
			return "UNKNOW";
		}
	};
}