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
#include <iomanip>
#include <chrono>
//跨平台兼容
#ifdef  _WIN32
#define LOCAL_TIME_SAFE(timestamp,tm_ptr) localtime_s(tm_ptr,&(timestamp))
#else
#define LOCAL_TIME_SAFE(timestamp,tm_ptr) localtime_r(&(timestamp),tm_ptr)
#endif //  _WIN32


namespace my_log {
	/**
	 * @brief 日志落地基类，定义日志输出的统一接口
	 */
	class LogSink {
	public:
		virtual ~LogSink() = default;

		/**
		 * @brief 纯虚函数，用于将日志数据写入具体的目的地
		 * @param data 指向待写入日志数据的指针
		 * @param len 待写入日志数据的长度
		 */
		virtual void log(const char* data, size_t len) = 0;
		using ptr = std::shared_ptr<LogSink>;
	};

	/**
	 * @brief 标准输出日志落地类，将日志打印到控制台
	 */
	class StdoutSink :public LogSink {
	public:
		/**
		 * @brief 将日志数据写入标准输出流
		 * @param data 指向待写入日志数据的指针
		 * @param len 待写入日志数据的长度
		 */
		void log(const char* data, size_t len) override
		{
			std::cout.write(data, len);
		}
	};

	/**
	 * @brief 文件日志落地类，将日志写入指定的单个文件
	 */
	class FileSink :public LogSink {
	public:
		/**
		 * @brief 构造函数，初始化文件路径并打开文件句柄
		 * @param pathname 日志文件的完整路径名
		 */
		FileSink(const std::string& pathname) :_pathname(pathname)
		{
			//1创建日志所在目录
			util::File::createdirectory(util::File::Path(pathname));
			//2创建并且打开日志文件
			_ofs.open(pathname,std::ios::binary | std::ios::app);
			assert(_ofs.is_open());
		}

		/**
		 * @brief 将日志消息追加写入指定文件
		 * @param data 指向待写入日志数据的指针
		 * @param len 待写入日志数据的长度
		 */
		void log(const char* data, size_t len) override
		{
			_ofs.write(data, len);
			assert(_ofs.good());
		}
	private:
		std::string _pathname;
		std::ofstream _ofs;
	};

	/**
	 * @brief 滚动文件日志落地类，根据文件大小自动切换日志文件
	 */
	class RollSizeSink : public LogSink {
	public:
		/**
		 * @brief 构造函数，初始化滚动文件策略
		 * @param basename 日志文件的基础名称前缀
		 * @param max_size 单个日志文件的最大允许大小（字节），超过此大小将创建新文件
		 */
		RollSizeSink(const std::string& basename, size_t max_size) :
			_basename(basename),
			_max_size(max_size),
			_cur_size(0),
			_name_count(0)
		{
			std::string pathname = createNewFile();
			util::File::createdirectory(util::File::Path(pathname));
			_ofs.open(pathname, std::ios::binary | std::ios::app);
			assert(_ofs.is_open());
		}

		/**
		 * @brief 将日志数据写入当前文件，若超出大小限制则滚动创建新文件
		 * @param data 指向待写入日志数据的指针
		 * @param len 待写入日志数据的长度
		 */
		void log(const char* data, size_t len)override
		{
			if (_cur_size >= _max_size)
			{
				std::string pathname = createNewFile();
				_ofs.close();//关闭之前打开文件的描述符
				_ofs.open(pathname, std::ios::binary | std::ios::app);
				assert(_ofs.is_open());
				_cur_size = 0;
			}
			_ofs.write(data, len);
			assert(_ofs.good());
			_cur_size += len;
		}
	private:
		/**
		 * @brief 生成新的滚动日志文件名，包含时间戳和序列号
		 * @return 新生成的日志文件完整路径字符串
		 */
		std::string createNewFile()
		{
			//获取系统时间,用时间来构建新文件名
			time_t t = util::Date::GetTime();
			struct tm lt{};
			LOCAL_TIME_SAFE(t, &lt);
			std::stringstream filename;
			filename << _basename;
			//setfill用来设置填充字符，setw用来设置字段宽度，默认右对齐,头文件iomanip
			filename << std::setw(4) << std::setfill('0') << lt.tm_year + 1900 << ".";
			filename << std::setw(2) << std::setfill('0') << lt.tm_mon + 1 << ".";
			filename << std::setw(2) << std::setfill('0') << lt.tm_mday << "-";
			filename << std::setw(2) << std::setfill('0') << lt.tm_hour << "-";
			filename << std::setw(2) << std::setfill('0') << lt.tm_min << "-";
			filename << std::setw(2) << std::setfill('0') << lt.tm_sec << "-";
			filename << _name_count++;
			filename << ".log";
			return filename.str();
		}
	private:
		//基础文件名+扩展文件名(以时间生成)组成一个实际的当前输出文件名
		size_t _name_count;
		std::string _basename;	//./logs/base-
		std::ofstream _ofs;
		size_t _max_size;//记录最大大小,当前文件超过这个大小就要切换文件
		size_t _cur_size;//记录当前文件已经写入的数据大小
	};

	/**
	 * @brief 日志落地对象工厂类，用于创建不同类型的LogSink实例
	 */
	class SinkFactory {
	public:
		/**
		 * @brief 模板静态方法，创建指定类型的LogSink共享指针
		 * @tparam SinkType 要创建的LogSink派生类类型
		 * @tparam Args 构造函数参数类型包
		 * @param args 传递给SinkType构造函数的参数包
		 * @return 指向新创建的LogSink对象的std::shared_ptr
		 */
		template<typename SinkType, typename ...Args>
		static LogSink::ptr create(Args &&...args)
		{
			return std::make_shared<SinkType>(std::forward<Args>(args)...);
		}
	};
}