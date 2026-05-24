/*实现异步工作器*/
#pragma once

#include "buffer.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <atomic>

namespace my_log {
	using Functor = std::function<void(Buffer&)>;
	enum class AsyncType {
		ASYNC_SAFE,//缓冲区满了就阻塞
		ASYNC_UNSAFE//不考虑资源，无限扩容
	};
	class AsyncLooper {
	public:
		using ptr = std::shared_ptr<AsyncLooper>;
		AsyncLooper(const Functor &cb,AsyncType loop_type = AsyncType::ASYNC_SAFE):
			_callback(cb),
			_loopr_type(loop_type),
			_stop(false),
			_thread(std::thread(&AsyncLooper::threadEntry,this))
		{ }
		~AsyncLooper() { stop(); }
		void stop() noexcept
		{
			_stop = true;
			_cond_con.notify_all();//唤醒所有工作线程

			try {
				if (_thread.joinable())//不加可能导致智能指针析构两次
					_thread.join();//等待工作线程的退出
			}
			catch (...) {
				//忽略所有异常，确保正常析构
			}
		}
		void push(const char* data, size_t len)
		{
			//1无限扩容（不安全）;2固定大小--生产缓冲区满就阻塞
			std::unique_lock<std::mutex> lock(_mutex);
			//条件变量控制，若缓冲区剩余大小大于数据长度就可以添加
			if(_loopr_type==AsyncType::ASYNC_SAFE)
				_cond_pro.wait(lock, [&]() {return _pro_buf.writeAbleSize() >= len; });
			//满足添加数据条件
			_pro_buf.push(data, len);
			//唤醒消费者对缓冲区中的数据进行处理
			_cond_con.notify_one();
		}
	private:
		//线程函数入口--对消费缓冲区中的数据进行处理，处理完毕后，初始化缓冲区，交换缓冲区
		void threadEntry()
		{
			while (1)
			{
				//1判断生成缓冲区有没有数据,有就交换，没有就阻塞
				//互斥锁生命周期
				{
					std::unique_lock<std::mutex> lock(_mutex);
					//退出前被唤醒，或者有数据被唤醒，则返回真，否则返回假
					if (_stop && _pro_buf.empty())break;
					_cond_con.wait(lock, [&]() {return _stop || !_pro_buf.empty(); });
					_con_buf.swap(_pro_buf);
					//2唤醒生产者
					if (_loopr_type == AsyncType::ASYNC_SAFE)
						 _cond_pro.notify_all();
				}
				//3被唤醒后，对消费缓冲区进行数据处理
				if (_callback) {
					_callback(_con_buf);
				}
				//4初始化消费缓冲区
				_con_buf.reset();
			}
		}
	private:
		Functor _callback;//具体对缓冲区数据进行处理的回调函数，由异步工作器使用者传入
	private:
		AsyncType _loopr_type;
		std::atomic<bool> _stop;//停止标志
		Buffer _pro_buf;//生产缓冲区
		Buffer _con_buf;//消费缓冲区
		std::mutex _mutex;
		std::condition_variable _cond_pro;
		std::condition_variable _cond_con;
		std::thread _thread;//异步日志工作器工作线程
	};
}//namespace my_log