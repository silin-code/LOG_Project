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
#include <sstream>
#include <cassert>
#include <ctime>
//跨平台兼容
#ifdef  _WIN32
#define LOCAL_TIME_SAFE(timestamp,tm_ptr) localtime_s(tm_ptr,&(timestamp))
#else
#define LOCAL_TIME_SAFE(timestamp,tm_ptr) localtime_r(&(timestamp),tm_ptr)
#endif //  _WIN32


namespace my_log {
	class LogSink {
	public:
		virtual ~LogSink(){}
		virtual void log(const char* data, size_t len) = 0;
	};

	//落地方向:标准输出
	class StdoutSink :public LogSink {
	public:
		void log(const char* data, size_t len)
		{
			std::cout.write(data, len);
		}
	};
	//落地方向:指定文件
	class FileSink :public LogSink {
	public:
		//构造时传入文件名,并打开文件,将操作句柄管理起来
		FileSink(const std::string& pathname) :_pathname(pathname)
		{
			//1创建日志所在目录
			util::File::createdirectory(util::File::Path(pathname));
			//2创建并且打开日志文件
			_ofs.open(pathname,std::ios::binary | std::ios::app);
			assert(_ofs.is_open());
		}
		//将日志消息写入到标准输出
		void log(const char* data, size_t len)
		{
			_ofs.write(data, len);
			assert(_ofs.good());
		}
	private:
		std::string _pathname;
		std::ofstream _ofs;
	};
	//落地方向:滚动文件(以大小滚动)
	class RollSizeSink : public LogSink {
	public:
		RollSizeSink(const std::string& basename, size_t max_size) :
			_basename(basename),
			_max_size(max_size),
			_cur_size(0)
		{
			std::string pathname = createNewFile();
			_ofs.open(pathname, std::ios::binary | std::ios::app);
			assert(_ofs.is_open());
		}

		void log(const char* data, size_t len)
		{
			if (_cur_size >= _max_size)
			{
				std::string pathname = createNewFile();
				_ofs.close();//关闭之前打开文件的描述符
				_ofs.open(pathname, std::ios::binary | std::ios::app);
				assert(_ofs.is_open());
			}
			_ofs.write(data, len);
			assert(_ofs.good());
			_cur_size += len;
		}
	private:
		//进行大小判断,超过指定大小责创建新文件
		std::string createNewFile()
		{
			//获取系统时间,用时间来构建新文件名
			time_t t = util::Date::GetTime();
			struct tm lt{};
			LOCAL_TIME_SAFE(t, &lt);
			std::stringstream filename;
			filename << _basename;
			filename << lt.tm_year;
			filename << lt.tm_mon;
			filename << lt.tm_mday;
			filename << lt.tm_hour;
			filename << lt.tm_min;
			filename << lt.tm_sec;
			filename << ".log";
			return filename.str();
		}
	private:
		//基础文件名+扩展文件名(以时间生成)组成一个实际的当前输出文件名
		std::string _basename;	//./logs/base-
		std::ofstream _ofs;
		size_t _max_size;//记录最大大小,当前文件超过这个大小就要切换文件
		size_t _cur_size;//记录当前文件已经写入的数据大小
	};

	class SinkFactory {

	};
}