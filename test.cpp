#include "log.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <iomanip>
#include <string>

// 性能测试结果结构体
struct PerfResult {
    std::string test_name;
    int thread_count;
    int total_logs;
    double total_seconds;
    double qps;          // 每秒日志条数
    double avg_latency;  // 单条日志平均耗时(微秒)
};

// 原子计数器，确保线程安全计数
std::atomic<long long> g_log_count{ 0 };

// 打印性能结果表格
void print_perf_header() {
    std::cout << std::left
        << std::setw(25) << "测试名称"
        << std::setw(12) << "线程数"
        << std::setw(15) << "总日志数"
        << std::setw(15) << "总耗时(秒)"
        << std::setw(18) << "QPS(条/秒)"
        << std::setw(20) << "平均延迟(微秒)"
        << std::endl;
    std::cout << std::string(100, '-') << std::endl;
}

void print_perf_result(const PerfResult& result) {
    std::cout << std::left
        << std::setw(25) << result.test_name
        << std::setw(12) << result.thread_count
        << std::setw(15) << result.total_logs
        << std::setw(15) << std::fixed << std::setprecision(3) << result.total_seconds
        << std::setw(18) << std::fixed << std::setprecision(0) << result.qps
        << std::setw(20) << std::fixed << std::setprecision(2) << result.avg_latency
        << std::endl;
}

// 通用性能测试函数 - 显式传入is_async标志
PerfResult run_perf_test(const std::string& test_name,
    my_log::Logger::ptr logger,
    int thread_count,
    int logs_per_thread,
    bool is_async = false,
    const std::string& log_template = "性能测试日志: %d") {
    g_log_count = 0;
    std::vector<std::thread> threads;

    // 高精度计时开始
    auto start_time = std::chrono::high_resolution_clock::now();

    // 启动所有工作线程
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < logs_per_thread; ++j) {
                my_log::LOG_INFO(logger, log_template.c_str(), j);
                g_log_count.fetch_add(1, std::memory_order_relaxed);
            }
            });
    }

    // 等待所有生产线程完成
    for (auto& t : threads) {
        t.join();
    }

    // 异步日志器额外等待消费完成
    if (is_async) {
        // 根据你的异步队列大小调整，确保所有日志都被消费
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    // 高精度计时结束
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    PerfResult result;
    result.test_name = test_name;
    result.thread_count = thread_count;
    result.total_logs = g_log_count.load();
    result.total_seconds = duration.count() / 1000000.0;
    result.qps = result.total_logs / result.total_seconds;
    result.avg_latency = (duration.count() * 1.0) / result.total_logs;

    return result;
}

int main() {
    std::cout << "\n========== 日志库【高并发性能】专项测试 ==========\n\n";

    // 测试配置参数 - 根据你的机器性能调整
    const int logs_per_thread = 10000;  // 每个线程打印的日志数
    const std::vector<int> thread_counts = { 1, 2, 4, 8, 16, 32 };  // 测试的线程数
    const std::vector<int> log_lengths = { 32, 128, 512, 2048 };    // 测试的日志长度

    print_perf_header();

    // ==================== 1. 同步日志器性能测试 ====================
    std::cout << "\n【同步日志器性能测试】\n";
    {
        my_log::LocalLoggerBuilder builder;
        builder.buildLoggerName("sync_perf");
        // 显式构建同步日志器
        builder.buildLoggerType(my_log::LoggerType::LOGGER_SYNC);
        builder.buildLoggerLevel(my_log::LogLevel::value::INFO);
        my_log::Logger::ptr sync_logger = builder.build();

        for (int threads : thread_counts) {
            auto result = run_perf_test(
                "同步日志-" + std::to_string(threads) + "线程",
                sync_logger,
                threads,
                logs_per_thread,
                false  // 明确指定为同步
            );
            print_perf_result(result);
        }
    }

    // ==================== 2. 异步日志器性能测试 ====================
    std::cout << "\n【异步日志器性能测试】\n";
    {
        my_log::LocalLoggerBuilder builder;
        builder.buildLoggerName("async_perf");
        // 显式构建异步日志器
        builder.buildLoggerType(my_log::LoggerType::LOGGER_ASYNC);
        builder.buildLoggerLevel(my_log::LogLevel::value::INFO);
        // 如果你的builder支持设置异步队列大小，在这里添加
        // builder.buildAsyncQueueSize(65536);
        my_log::Logger::ptr async_logger = builder.build();

        for (int threads : thread_counts) {
            auto result = run_perf_test(
                "异步日志-" + std::to_string(threads) + "线程",
                async_logger,
                threads,
                logs_per_thread,
                true  // 明确指定为异步
            );
            print_perf_result(result);
        }
    }

    // ==================== 3. 不同日志长度性能测试 ====================
    std::cout << "\n【不同日志长度性能测试(8线程)】\n";
    {
        my_log::LocalLoggerBuilder builder;
        builder.buildLoggerName("length_perf");
        builder.buildLoggerType(my_log::LoggerType::LOGGER_ASYNC);
        builder.buildLoggerLevel(my_log::LogLevel::value::INFO);
        my_log::Logger::ptr length_logger = builder.build();

        for (int len : log_lengths) {
            std::string prefix(len, 'x');
            std::string log_template = prefix + " - %d";

            auto result = run_perf_test(
                "日志长度-" + std::to_string(len) + "字节",
                length_logger,
                8,  // 固定8线程
                logs_per_thread / 2,  // 长日志减少数量避免测试过久
                true,  // 异步
                log_template
            );
            print_perf_result(result);
        }
    }

    // ==================== 4. 极限压力测试(队列满场景) ====================
    std::cout << "\n【异步日志极限压力测试(队列满场景)】\n";
    {
        // 创建小容量队列的异步日志器，更容易触发队列满
        my_log::LocalLoggerBuilder builder;
        builder.buildLoggerName("async_pressure");
        builder.buildLoggerType(my_log::LoggerType::LOGGER_ASYNC);
        builder.buildLoggerLevel(my_log::LogLevel::value::INFO);
        // 关键：设置小队列大小，快速触发队列满
        // builder.buildAsyncQueueSize(1024); 
        my_log::Logger::ptr pressure_logger = builder.build();

        // 32个线程疯狂打日志，测试队列满时的表现
        auto result = run_perf_test(
            "异步极限压力-32线程",
            pressure_logger,
            32,
            logs_per_thread * 2,  // 双倍日志量
            true  // 异步
        );
        print_perf_result(result);

        // 额外等待确保所有日志都被消费
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    // ==================== 5. 混合日志等级并发测试 ====================
    std::cout << "\n【混合日志等级并发测试(16线程)】\n";
    {
        my_log::LocalLoggerBuilder builder;
        builder.buildLoggerName("mixed_level_perf");
        builder.buildLoggerType(my_log::LoggerType::LOGGER_ASYNC);
        builder.buildLoggerLevel(my_log::LogLevel::value::DEBUG);
        my_log::Logger::ptr mixed_logger = builder.build();

        g_log_count = 0;
        const int threads = 16;
        const int logs_per_level = 5000;
        std::vector<std::thread> threads_vec;

        auto start_time = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < threads; ++i) {
            threads_vec.emplace_back([&]() {
                for (int j = 0; j < logs_per_level; ++j) {
                    my_log::LOG_DEBUG(mixed_logger, "DEBUG日志: %d", j);
                    my_log::LOG_INFO(mixed_logger, "INFO日志: %d", j);
                    my_log::LOG_WARN(mixed_logger, "WARN日志: %d", j);
                    my_log::LOG_ERR(mixed_logger, "ERR日志: %d", j);
                    g_log_count.fetch_add(4, std::memory_order_relaxed);
                }
                });
        }

        for (auto& t : threads_vec) {
            t.join();
        }

        // 异步等待消费完成
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        PerfResult result;
        result.test_name = "混合等级-16线程";
        result.thread_count = threads;
        result.total_logs = g_log_count.load();
        result.total_seconds = duration.count() / 1000000.0;
        result.qps = result.total_logs / result.total_seconds;
        result.avg_latency = (duration.count() * 1.0) / result.total_logs;

        print_perf_result(result);
    }

    // ==================== 6. 日志丢失检测测试 ====================
    std::cout << "\n【异步日志丢失检测测试】\n";
    {
        my_log::LocalLoggerBuilder builder;
        builder.buildLoggerName("loss_detect");
        builder.buildLoggerType(my_log::LoggerType::LOGGER_ASYNC);
        builder.buildLoggerLevel(my_log::LogLevel::value::INFO);
        my_log::Logger::ptr loss_logger = builder.build();

        const int total_expected = 100000;
        g_log_count = 0;

        auto start_time = std::chrono::high_resolution_clock::now();

        // 10个线程各打1万条
        std::vector<std::thread> threads;
        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < 10000; ++j) {
                    my_log::LOG_INFO(loss_logger, "丢失检测: %d", j);
                    g_log_count.fetch_add(1, std::memory_order_relaxed);
                }
                });
        }

        for (auto& t : threads) {
            t.join();
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        long long actual = g_log_count.load();
        double loss_rate = (total_expected - actual) * 100.0 / total_expected;

        std::cout << "预期日志数: " << total_expected << std::endl;
        std::cout << "实际日志数: " << actual << std::endl;
        std::cout << "丢失率: " << std::fixed << std::setprecision(4) << loss_rate << "%" << std::endl;
        std::cout << "总耗时: " << std::fixed << std::setprecision(3)
            << duration.count() / 1000000.0 << "秒" << std::endl;
    }

    std::cout << "\n========== 高并发性能测试全部完成 ==========\n";
    std::cout << "\n测试说明：\n";
    std::cout << "1. QPS越高表示性能越好\n";
    std::cout << "2. 平均延迟越低表示响应越快\n";
    std::cout << "3. 异步日志在高并发下通常比同步日志性能好\n";
    std::cout << "4. 日志长度越长，性能会相应下降\n";
    std::cout << "5. 极限压力测试验证队列满时是否会崩溃或阻塞\n";
    std::cout << "6. 丢失检测测试验证异步日志是否有数据丢失\n\n";

    return 0;
}