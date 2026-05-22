/*实现异步日志缓冲区*/
#pragma once

#include "util.hpp"
#include "vector"
#include <cassert>
namespace my_log{

	#define DEFAULT_BUFFER_SIZE (1*1024*1024)
	#define THRESHOLD_BUFFER_SIZE (8*1024*1024)
	#define INCREMENT_BUFFER_SIZE (1*1024*1024)
	class Buffer {
	public:
		Buffer():_buffer(DEFAULT_COMPARTMENT_ID),_write_idx(0),_read_idx(0){}

		~Buffer() = default;

		//向缓冲区写数据
		void push(const char* data, size_t len)
		{
			//缓冲区大小不够:1扩容，2阻塞返回false
			//1固定大小直接返回
			//if (len > writeAbleSize()) return;
			//2扩容
			ensureEnoughSize(len);
			//将数据写入缓冲区
			std::copy(data, data + len, &_buffer[_write_idx]);
			//将当前写入位置向后偏移
			moveWriter(len);
		}

		//剩余可写空间大小
		size_t writeAbleSize()
		{
			//对于扩容思路，不存在可写空间大小，这个只是固定缓冲区接口
			return (_buffer.size() - _write_idx);
		}

		//返回可读数据的起始地址
		const char* begin()
		{
			return &_buffer[_read_idx];
		}

		//返回可读数据的长度,实际数据长度
		size_t readAbleSize()
		{
			return (_write_idx - _read_idx);
		}

		//对读指针进行偏移
		void moveReader(size_t len)
		{
			assert(len <= readAbleSize());
			{
				_read_idx += len;
			}
		}

		//对读写指针进行重置
		void reset()
		{
			_write_idx = 0;//缓冲区空间全部空闲
			_read_idx = 0;//没有可读区域
		}

		//对Buffer实现交换
		void swap(Buffer& buffer)
		{
			_buffer.swap(buffer._buffer);
			std::swap(_read_idx, buffer._read_idx);
			std::swap(_write_idx, buffer._write_idx);
		}

		//判断缓冲区是否为空
		bool empty()
		{
			return _read_idx == _write_idx;
		}
	private:
		//对空间扩容
		void ensureEnoughSize(size_t len)
		{
			if (len < writeAbleSize()) return;//不用扩容
			size_t newsize = 0;
			if (_buffer.size() < THRESHOLD_BUFFER_SIZE)
			{
				newsize = _buffer.size() * 2;
			}
			else
			{
				newsize = _buffer.size() + INCREMENT_BUFFER_SIZE;
			}
			_buffer.resize(newsize);
		}

		//对读写指针进行向后偏移操作
		void moveWriter(size_t len)
		{
			assert(len+_write_idx <= _buffer.size());
			_write_idx += len;
		}

	private:
		std::vector<char> _buffer;
		size_t _read_idx;//当前可读数据的指针--本质是下标
		size_t _write_idx;//当前可写数据的指针
	};
}//namespace my_log