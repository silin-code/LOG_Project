#pragma once
#include "level.hpp"
#include "message.hpp"

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include <ctime>
#include <cassert>

//跨平台兼容
#ifdef  _WIN32
#define LOCAL_TIME_SAFE(timestamp,tm_ptr) localtime_s(tm_ptr,&(timestamp))
#else
#define LOCAL_TIME_SAFE(timestamp,tm_ptr) localtime_r(&(timestamp),tm_ptr)
#endif //  _WIN32



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
	//派生格式化子项子类--消息，等级，时间，文件名，行号，线程ID，日志器名，制表符，换行，其他
	class FormatItem{
	public:
		using ptr = std::shared_ptr<FormatItem>;
		virtual void format(std::ostream& out,const LogMsg& msg) = 0;
		virtual ~FormatItem() = default;
	};

	//日志消息内容 %m
	class MsgFormatItem :public FormatItem {
	public:
		void format(std::ostream& out, const LogMsg& msg)override
		{
			out << msg._payload;
		}
	};

	//日志等级 %p
	class LevelFormatItem :public FormatItem
	{
	public:
		void format(std::ostream& out,const LogMsg& msg)override
		{
			out << LogLevel::ToString(msg._level);
		}
	};

	//时间格式化 %d (支持自定义格式，默认 %H:%M:%S)
	class TimeFormatItem :public FormatItem
	{
	public:
		TimeFormatItem(const std::string &fmt="%H:%M:%S"):_time_fmt(fmt){}
		void format(std::ostream& out,const LogMsg& msg) override {
			struct tm local_tm {};
			LOCAL_TIME_SAFE(msg._ctime,&local_tm);
			char buf[64] = { 0 };
			strftime(buf, sizeof(buf), _time_fmt.c_str(), &local_tm);
			out << buf;
		}
	private:
		std::string _time_fmt;//%H:%M:%S
	};

	//文件名 %f
	class FileNameFormatItem : public FormatItem {
	public:
		void format(std::ostream& out, const LogMsg& msg) override { out << msg._filename; }
	};

	//行号 %l
	class LineFormatItem : public FormatItem {
	public:
		void format(std::ostream& out, const LogMsg& msg) override { out << msg._line; }
	};


	//线程ID %t
	class ThreadFormatItem : public FormatItem {
	public:
		void format(std::ostream& out, const LogMsg& msg) override {
			out << msg._tid;
		}
	};

	// 日志器名称 %c
	class LoggerFormatItem : public FormatItem {
	public:
		void format(std::ostream& out, const LogMsg& msg) override {
			out << msg._logger;
		}
	};

	// 制表符 %T
	class TabFormatItem : public FormatItem {
	public:
		void format(std::ostream& out, const LogMsg&) override {
			out << '\t';
		}
	}; 
	
	//换行符 %n
	class NLineFormatItem : public FormatItem {
	public:
		void format(std::ostream& out, const LogMsg&) override {
			out << '\n';
		}
	};

	// 普通字符串（非格式化字符）
	class OtherFormatItem : public FormatItem {
	public:
		OtherFormatItem(const std::string& str) :_str(str) {}
		void format(std::ostream& out, const LogMsg& msg) override
		{
			out << _str;
		}
	private:
		std::string _str;
	};

	class Formatter{
	public:
		Formatter(const std::string& pattern = "[%d{%H:%M:%S}][%t][%c][%f:%l][%p]%T%m%n"):
			_pattern(pattern){
			assert(parsePattern());
		}
		
		//对msg进行格式化
		std::string format(const LogMsg& msg)
		{
			std::stringstream ss;
			format(ss, msg);
			return ss.str();
		}
		void format(std::ostream& out, const LogMsg& msg)
		{
			for (auto &it: _items)
			{
				it->format(out, msg);
			}
		}
		//对格式化规则字符串进行解析
		bool parsePattern()
		{
			//1对格式化字符串进行解析
			// abcde%d{%H:%M:%S}][%p]%T%m%n
			std::vector<std::pair<std::string, std::string>> fmt_order;
			size_t pos = 0;
			while (pos < _pattern.size())
			{
				//1处理原始字符串--判断是否是%，不是就是原始字符
				std::string key, val;
				if (_pattern[pos] != '%')
				{
					val.push_back(_pattern[pos++]);
					continue;
				}

				//2pos就是%字符而且pos+1也为%
				if (pos + 1 < _pattern.size() && _pattern[pos + 1] == '%')
				{
					val.push_back('%');pos += 2;continue;
				}

				//3%后面是个格式化字符,代表原始字符串处理完毕
				if (val.empty()==false)
				{
					fmt_order.push_back(std::make_pair("", val));
					val.clear();
				}

				if (++pos == _pattern.size())
				{
					std::cout << "%之后,没有对应的格式化字符串\n";
					return false;
				}

				//格式化字符串处理
				key = _pattern[pos++];
				//pos指向格式化字符之后的位置
				if (pos<_pattern.size()&&_pattern[pos] == '{')
				{
					pos++;//这个时候pos指向子规则的起始位置
					while (pos < _pattern.size() && _pattern[pos] != '}')
					{
						val.push_back(_pattern[pos++]);
					}
					//走到末尾跳出循环，还没有遇到}
					if (pos == _pattern.size())
					{
						std::cout << "子括号{}匹配出错\n";
						return false;//没找到
					}
					pos++;//这个时候pos指向的是}位置,需要++去处理下一个位置
				}
				fmt_order.push_back(std::make_pair(key,val));
				key.clear();
				val.clear();
			}
			//2根据解析得到的数据初始化子项数组成员
			for (auto& it : fmt_order)
			{
				_items.push_back(createItem(it.first, it.second));
			}
			return true;
		}
	private:
		//根据不同的格式化字符创建不同的格式化子项对象
		FormatItem::ptr发 createItem(const std::string& key, const std::string& val)
		{
			if (key == "d") return std::make_shared<TimeFormatItem>(val);
			if (key == "t") return std::make_shared<ThreadFormatItem>();
			if (key == "c") return std::make_shared<LoggerFormatItem>();
			if (key == "f") return std::make_shared<FileNameFormatItem>();
			if (key == "l") return std::make_shared<LineFormatItem>();
			if (key == "p") return std::make_shared<LevelFormatItem>();
			if (key == "T") return std::make_shared<TabFormatItem>();
			if (key == "m") return std::make_shared<MsgFormatItem>();
			if (key == "n") return std::make_shared<NLineFormatItem>();
			return std::make_shared<OtherFormatItem>(val);
		}
	private:
		std::string _pattern;//格式化字符串
		std::vector<FormatItem::ptr> _items;
	};
}