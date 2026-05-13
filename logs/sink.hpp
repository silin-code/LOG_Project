#pragma once
/*
日志落地模块的实现
	1抽象落地基类
	2派生子类(根据不同的落地方向派生)
	3使用工厂模式进行创建与表示的分离
*/
#include "util.hpp"
#include <memory>
#include <fstream>

namespace my_log {
	class LogSink {
	public:
		virtual ~LogSink(){}
		virtual void log(const char* data, size_t len) = 0;
	};

	//落地方向:标准输出
	class StdoutSink :public LogSink {
	public:
		void log(const char* data, size_t len);
	};
	//落地方向:指定文件
	class FileSink :public LogSink {
	public:
		//构造时传入文件名,并打开文件,将操作句柄管理起来
		FileSink(const std::string& pathname);
		//将日志消息写入到标准输出
		void log(const char* data, size_t len);
	private:
		std::string _pathname;
		std::ofstream _ofs;
	};
	//落地方向:滚动文件(以大小滚动)
	class RollSizeSink : public LogSink {
	public:
	private:
		//基础文件名+扩展文件名(以时间生成)组成一个实际的当前输出文件名
		std::string _basename;	//./logs/base-
		std::ostream _ofs;
		size_t _max_size;//记录最大大小,当前文件超过这个大小就要切换文件
		size_t _cur_size;//记录当前文件已经写入的数据大小
	};

	class SinkFactory {

	};
}