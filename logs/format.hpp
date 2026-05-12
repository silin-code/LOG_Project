#pragma once
#include "level.hpp"
#include "message.hpp"


namespace my_log {
	//抽象格式化子类基类
	/*	
		size_t _ctime;//日志时间戳
		LogLevel::value _level;//日志等级
		std::string _filename;//源代码名称
		size_t _line;//行号
		std::thread::id _tid;//线程id
		std::string _logger;//日志器名称
		std::string _payload;//有效消息数据
	*/
	class FormatItem{
	public:
		using ptr = std::shared_ptr<FormatItem>;
		virtual void format(std::ostream& out,LogMsg& msg) = 0;
	};

	class MsgFormatItem :public FormatItem {
	public:
		void format(std::ostream& out, LogMsg& msg)override
		{

		}
	};

	class 
}