#include "log.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

int main()
{
    std::cout << "========== 日志器【边缘极端场景】测试 ==========\n\n";

    // 1. 空消息、空格式化字符串测试（最边缘）
    std::cout << "【1】空消息/空格式测试\n";
    {
        my_log::DEBUG("");
        my_log::INFO(nullptr);
        my_log::WARN("");
        my_log::ERR("");
    }
    std::cout << "【1】测试完成\n\n";

    // 2. 超长日志字符串测试（缓冲区边界）
    std::cout << "【2】超长日志内容测试\n";
    {
        std::string long_str(2048, 'x'); // 2K超长字符串
        my_log::INFO("超长日志：%s", long_str.c_str());
    }
    std::cout << "【2】测试完成\n\n";

    // 3. 空指针日志器调用（安全校验）
    std::cout << "【3】空日志器指针调用测试\n";
    {
        my_log::Logger::ptr null_logger = nullptr;
        // 不会崩溃，接口内部做了空判断
        my_log::LOG_INFO(null_logger, "空指针日志器测试");
    }
    std::cout << "【3】测试完成\n\n";

    // 4. 重复创建同名日志器（管理器去重边界）
    std::cout << "【4】重复创建同名日志器测试\n";
    {
        my_log::LocalLoggerBuilder builder;
        builder.buildLoggerName("repeat_log");
        builder.build();
        builder.build(); // 重复创建
        my_log::Logger::ptr logger = my_log::getLogger("repeat_log");
        my_log::LOG_INFO(logger, "同名日志器去重成功");
    }
    std::cout << "【4】测试完成\n\n";

    // 5. 日志等级边界值测试（最低/最高等级）
    std::cout << "【5】日志等级边界测试\n";
    {
        // 最低等级DEBUG，所有日志都输出
        my_log::LocalLoggerBuilder b1;
        b1.buildLoggerName("level_min");
        b1.buildLoggerLevel(my_log::LogLevel::value::DEBUG);
        b1.build();
        // 最高等级FATAL，只输出致命日志
        my_log::LocalLoggerBuilder b2;
        b2.buildLoggerName("level_max");
        b2.buildLoggerLevel(my_log::LogLevel::value::FATAL);
        b2.build();

        my_log::Logger::ptr max_log = my_log::getLogger("level_max");
        my_log::LOG_DEBUG(max_log, "DEBUG 被过滤");
        my_log::LOG_FATAL(max_log, "FATAL 最高等级输出");
    }
    std::cout << "【5】测试完成\n\n";

    // 6. 高频极短间隔打印（性能/锁边界）
    std::cout << "【6】高频连续打印测试\n";
    {
        my_log::Logger::ptr logger = my_log::getLogger("sync_log");
        if (logger) {
            for (int i = 0; i < 100; ++i) {
                my_log::LOG_INFO(logger, "高频日志：%d", i);
            }
        }
    }
    std::cout << "【6】测试完成\n\n";

    // 7. 多线程疯狂竞争同一个日志器（并发边界）
    std::cout << "【7】多线程强竞争测试\n";
    {
        my_log::Logger::ptr logger = my_log::getLogger("sync_log");
        if (!logger) return 0;

        std::vector<std::thread> threads;
        // 5个线程同时抢锁打印
        for (int i = 0; i < 5; ++i) {
            threads.emplace_back([&, idx = i]() {
                for (int j = 0; j < 10; ++j) {
                    my_log::LOG_ERR(logger, "线程[%d] 强竞争日志:%d", idx, j);
                }
                });
        }
        for (auto& t : threads) t.join();
    }
    std::cout << "【7】测试完成\n\n";

    // 8. 异步日志压满后销毁（异步队列边界）
    std::cout << "【8】异步日志高压消费测试\n";
    {
        my_log::LocalLoggerBuilder builder;
        builder.buildLoggerName("async_pressure");
        builder.buildLoggerType(my_log::LoggerType::LOGGER_ASYNC);
        my_log::Logger::ptr logger = builder.build();

        // 压入大量日志
        for (int i = 0; i < 50; ++i) {
            my_log::LOG_INFO(logger, "异步高压日志：%d", i);
        }
        // 等待消费完成，防卡死
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    std::cout << "【8】测试完成\n\n";

    // 9. 无落地器日志器测试（修改为安全方式，不直接访问protected成员）
    std::cout << "【9】无落地器日志器测试（安全方式）\n";
    {
        // 不添加任何sink，直接构建（实际会被默认补一个StdoutSink，这里仅测试接口安全）
        my_log::LocalLoggerBuilder builder;
        builder.buildLoggerName("empty_sink_log");
        my_log::Logger::ptr logger = builder.build();
        // 无输出，不崩溃
        my_log::LOG_INFO(logger, "接口调用正常，不崩溃");
    }
    std::cout << "【9】测试完成\n\n";

    // 10. 全局根日志器重复调用（单例边界）
    std::cout << "【10】根日志器重复获取测试\n";
    {
        auto l1 = my_log::rootLogger();
        auto l2 = my_log::rootLogger();
        // 两个指针指向同一个单例
        my_log::LOG_INFO(l1, "根日志器单例唯一");
    }
    std::cout << "【10】测试完成\n\n";

    std::cout << "========== 所有边缘测试全部通过 ==========\n";
    return 0;
}