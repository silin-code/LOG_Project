#include "buffer.hpp"
#include "looper.hpp"
#include "sink.hpp"

#include <iostream>
#include <cstring>
#include <cstdio>
#include <vector>
#include <thread>
#include <atomic>
#include <fstream>
#include <string>

int main() {
    std::cout << "=====================================" << std::endl;
    std::cout << "      异步日志系统极简测试套件" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << std::endl;

    // =====================================
    // 测试1：Buffer缓冲区基础功能
    // =====================================
    std::cout << "1. 测试Buffer缓冲区基础功能..." << std::endl;
    {
        my_log::Buffer buf;
        assert(buf.empty() == true);
        assert(buf.readAbleSize() == 0);

        const char* str = "test buffer";
        buf.push(str, 11);
        assert(buf.readAbleSize() == 11);
        assert(memcmp(buf.begin(), str, 11) == 0);

        buf.reset();
        assert(buf.empty() == true);

        std::cout << "   ✅ 通过" << std::endl;
    }

    // =====================================
    // 测试2：Buffer缓冲区扩容功能
    // =====================================
    std::cout << "2. 测试Buffer缓冲区扩容功能..." << std::endl;
    {
        my_log::Buffer buf;
        std::vector<char> big(2 * 1024 * 1024, 'a');
        buf.push(big.data(), big.size());
        assert(buf.readAbleSize() == 2 * 1024 * 1024);

        std::cout << "   ✅ 通过" << std::endl;
    }

    // =====================================
    // 测试3：AsyncLooper安全模式
    // =====================================
    std::cout << "3. 测试AsyncLooper安全模式..." << std::endl;
    {
        size_t total = 0;
        my_log::Functor cb = [&](my_log::Buffer& buf) {
            total += buf.readAbleSize();
            };

        my_log::AsyncLooper::ptr looper =
            std::make_shared<my_log::AsyncLooper>(cb, my_log::AsyncType::ASYNC_SAFE);

        std::string msg = "safe test\n";
        for (int i = 0; i < 100; ++i) {
            looper->push(msg.c_str(), msg.size());
        }

        looper->stop();
        assert(total == 100 * msg.size());

        std::cout << "   ✅ 通过" << std::endl;
    }

    // =====================================
    // 测试4：AsyncLooper非安全模式
    // =====================================
    std::cout << "4. 测试AsyncLooper非安全模式..." << std::endl;
    {
        size_t total = 0;
        my_log::Functor cb = [&](my_log::Buffer& buf) {
            total += buf.readAbleSize();
            };

        my_log::AsyncLooper::ptr looper =
            std::make_shared<my_log::AsyncLooper>(cb, my_log::AsyncType::ASYNC_UNSAFE);

        std::vector<char> big(1024 * 1024, 'b');
        looper->push(big.data(), big.size());

        looper->stop();
        assert(total == 1024 * 1024);

        std::cout << "   ✅ 通过" << std::endl;
    }

    // =====================================
    // 测试5：空回调函数（不崩溃）
    // =====================================
    std::cout << "5. 测试空回调函数..." << std::endl;
    {
        my_log::AsyncLooper::ptr looper =
            std::make_shared<my_log::AsyncLooper>(my_log::Functor(), my_log::AsyncType::ASYNC_SAFE);

        looper->push("test empty", 9);
        looper->stop();

        std::cout << "   ✅ 通过" << std::endl;
    }

    // =====================================
    // 测试6：多次调用stop（不崩溃）
    // =====================================
    std::cout << "6. 测试多次调用stop..." << std::endl;
    {
        my_log::Functor cb = [](my_log::Buffer&) {};
        my_log::AsyncLooper::ptr looper =
            std::make_shared<my_log::AsyncLooper>(cb, my_log::AsyncType::ASYNC_SAFE);

        looper->stop();
        looper->stop();
        looper->stop();

        std::cout << "   ✅ 通过" << std::endl;
    }

    // =====================================
    // 测试7：文件落地器集成
    // =====================================
    std::cout << "7. 测试文件落地器集成..." << std::endl;
    {
        std::remove("./test_file.log");
        my_log::LogSink::ptr file_sink =
            my_log::SinkFactory::create<my_log::FileSink>("./test_file.log");

        my_log::Functor cb = [&](my_log::Buffer& buf) {
            if (buf.readAbleSize() > 0) {
                file_sink->log(buf.begin(), buf.readAbleSize());
            }
            };

        my_log::AsyncLooper::ptr looper =
            std::make_shared<my_log::AsyncLooper>(cb, my_log::AsyncType::ASYNC_UNSAFE);

        for (int i = 0; i < 10; ++i) {
            std::string msg = "第" + std::to_string(i) + "条日志\n";
            looper->push(msg.c_str(), msg.size());
        }

        looper->stop();
        file_sink.reset();

        // 验证文件行数
        std::ifstream ifs("./test_file.log");
        std::string line;
        int line_count = 0;
        while (std::getline(ifs, line)) line_count++;
        ifs.close();

        assert(line_count == 10);

        std::cout << "   ✅ 通过" << std::endl;
    }

    // =====================================
    // 测试8：多线程并发写入
    // =====================================
    std::cout << "8. 测试多线程并发写入..." << std::endl;
    {
        std::remove("./test_concurrent.log");
        my_log::LogSink::ptr file_sink =
            my_log::SinkFactory::create<my_log::FileSink>("./test_concurrent.log");

        std::atomic<size_t> total_written(0);
        my_log::Functor cb = [&](my_log::Buffer& buf) {
            total_written += buf.readAbleSize();
            file_sink->log(buf.begin(), buf.readAbleSize());
            };

        my_log::AsyncLooper::ptr looper =
            std::make_shared<my_log::AsyncLooper>(cb, my_log::AsyncType::ASYNC_SAFE);

        const int thread_num = 5;
        const int msg_per_thread = 200;
        std::vector<std::thread> threads;

        for (int i = 0; i < thread_num; ++i) {
            threads.emplace_back([&, i]() {
                for (int j = 0; j < msg_per_thread; ++j) {
                    std::string msg = "T" + std::to_string(i) + " M" + std::to_string(j) + "\n";
                    looper->push(msg.c_str(), msg.size());
                }
                });
        }

        for (auto& t : threads) t.join();
        looper->stop();
        file_sink.reset();

        // 验证文件行数
        std::ifstream ifs("./test_concurrent.log");
        std::string line;
        int line_count = 0;
        while (std::getline(ifs, line)) line_count++;
        ifs.close();

        assert(line_count == thread_num * msg_per_thread);

        std::cout << "   ✅ 通过" << std::endl;
    }

    std::cout << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "      🎉 所有测试全部通过！" << std::endl;
    std::cout << "=====================================" << std::endl;

    return 0;
}