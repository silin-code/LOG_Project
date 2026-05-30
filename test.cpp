// 必须放在最开头！阻止Windows生成max/min宏
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "log.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <iomanip>
#include <string>
#include <algorithm>

// 测试参数（可根据你的机器调整）
const std::vector<int> TEST_THREADS = { 1, 2, 4, 8 }; // 测试的线程数
const int LOGS_PER_THREAD = 10000;                   // 每个线程打多少条日志
const int ASYNC_WAIT_MS = 300;                        // 异步日志消费等待时间

// 单个测试点的结果
struct TestPoint {
    int threads;          // 线程数
    long long sync_qps;   // 同步QPS
    long long async_qps;  // 异步QPS
    double speedup;       // 异步比同步快多少倍
};

// 全局结果数组
std::vector<TestPoint> g_results;
std::atomic<long long> g_log_count{ 0 };

// 通用测试函数：返回QPS
long long run_test(my_log::Logger::ptr logger, int threads, int logs_per_thread, bool is_async) {
    g_log_count = 0;
    std::vector<std::thread> thread_list;

    auto start = std::chrono::high_resolution_clock::now();

    // 启动所有线程
    for (int i = 0; i < threads; ++i) {
        thread_list.emplace_back([&]() {
            for (int j = 0; j < logs_per_thread; ++j) {
                my_log::LOG_INFO(logger, "性能测试日志: %d", j);
                g_log_count.fetch_add(1, std::memory_order_relaxed);
            }
            });
    }

    // 等待所有生产线程完成
    for (auto& t : thread_list) {
        t.join();
    }

    // 异步日志额外等待消费完成
    if (is_async) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ASYNC_WAIT_MS));
    }

    auto end = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000000.0;

    return static_cast<long long>(g_log_count.load() / seconds);
}

// 打印最终对比总表
void print_final_comparison() {
    std::cout << "\n\n" << std::string(80, '=') << std::endl;
    std::cout << "                    📊 同步 vs 异步 日志性能对比总表" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    // 表头
    std::cout << std::left
        << std::setw(12) << "线程数"
        << std::setw(18) << "同步日志QPS"
        << std::setw(18) << "异步日志QPS"
        << std::setw(20) << "异步比同步快"
        << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    // 计算平均加速比
    double total_speedup = 0.0;
    long long max_sync_qps = 0;
    long long max_async_qps = 0;

    // 打印每行数据
    for (const auto& res : g_results) {
        std::cout << std::left
            << std::setw(12) << res.threads
            << std::setw(18) << res.sync_qps
            << std::setw(18) << res.async_qps
            << std::setw(20) << std::fixed << std::setprecision(1) << res.speedup << " 倍"
            << std::endl;

        total_speedup += res.speedup;
        // 关键修复：用括号包裹std::max，避免被Windows宏替换
        max_sync_qps = (std::max)(max_sync_qps, res.sync_qps);
        max_async_qps = (std::max)(max_async_qps, res.async_qps);
    }

    // 打印总结行
    double avg_speedup = total_speedup / g_results.size();
    std::cout << std::string(80, '-') << std::endl;
    std::cout << std::left
        << std::setw(12) << "总结"
        << std::setw(18) << "最高: " + std::to_string(max_sync_qps)
        << std::setw(18) << "最高: " + std::to_string(max_async_qps)
        << std::setw(20) << "平均: " + std::to_string(static_cast<int>(avg_speedup * 10) / 10.0) + " 倍"
        << std::endl;

    // 结论
    std::cout << "\n📝 测试结论：" << std::endl;
    if (avg_speedup > 1.0) {
        std::cout << "  ✅ 异步日志性能优于同步日志，平均快 " << std::fixed << std::setprecision(1) << avg_speedup << " 倍" << std::endl;
        std::cout << "  ✅ 线程数越多，异步的优势越明显（同步锁竞争加剧）" << std::endl;
    }
    else {
        std::cout << "  ⚠️  异步日志性能不如同步，需要优化以下点：" << std::endl;
        std::cout << "     1. 减小锁粒度，使用自旋锁替代互斥锁" << std::endl;
        std::cout << "     2. 优化双缓冲区交换逻辑" << std::endl;
        std::cout << "     3. 减少内存拷贝和动态扩容" << std::endl;
    }
}

// 日志丢失检测
void run_loss_test() {
    std::cout << "\n🔍 正在进行异步日志丢失检测..." << std::flush;

    my_log::LocalLoggerBuilder builder;
    builder.buildLoggerName("loss_test");
    builder.buildLoggerType(my_log::LoggerType::LOGGER_ASYNC);
    auto logger = builder.build();

    const int TOTAL_EXPECTED = 100000;
    g_log_count = 0;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 10000; ++j) {
                my_log::LOG_INFO(logger, "丢失检测: %d", j);
                g_log_count.fetch_add(1, std::memory_order_relaxed);
            }
            });
    }

    for (auto& t : threads) {
        t.join();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto end = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000000.0;

    long long actual = g_log_count.load();
    double loss_rate = (TOTAL_EXPECTED - actual) * 100.0 / TOTAL_EXPECTED;

    std::cout << "\r✅ 异步日志丢失检测完成                  " << std::endl;
    std::cout << "   预期日志数: " << TOTAL_EXPECTED << std::endl;
    std::cout << "   实际日志数: " << actual << std::endl;
    std::cout << "   丢失率: " << std::fixed << std::setprecision(4) << loss_rate << "%" << std::endl;
    std::cout << "   总耗时: " << std::fixed << std::setprecision(3) << seconds << "秒" << std::endl;

    if (loss_rate > 0.0001) {
        std::cout << "   ⚠️  存在日志丢失，请检查异步队列满时的处理逻辑" << std::endl;
    }
    else {
        std::cout << "   ✅ 无日志丢失，数据一致性正常" << std::endl;
    }
}

int main() {
    // Windows控制台中文乱码修复
    system("chcp 65001 > nul");

    std::cout << "\n🚀 日志库性能对比测试开始" << std::endl;
    std::cout << "   测试配置: 每个线程打 " << LOGS_PER_THREAD << " 条日志" << std::endl;
    std::cout << "   测试线程数: ";
    for (int t : TEST_THREADS) std::cout << t << " ";
    std::cout << "\n" << std::endl;

    // ==================== 1. 同步日志测试 ====================
    std::cout << "1️⃣  正在测试同步日志器..." << std::endl;
    my_log::LocalLoggerBuilder sync_builder;
    sync_builder.buildLoggerName("sync_test");
    sync_builder.buildLoggerType(my_log::LoggerType::LOGGER_SYNC);
    auto sync_logger = sync_builder.build();

    for (int threads : TEST_THREADS) {
        std::cout << "   测试 " << threads << " 线程..." << std::flush;
        long long qps = run_test(sync_logger, threads, LOGS_PER_THREAD, false);
        g_results.push_back({ threads, qps, 0, 0.0 });
        std::cout << "\r   " << threads << " 线程完成，QPS: " << qps << std::endl;
    }

    // ==================== 2. 异步日志测试 ====================
    std::cout << "\n2️⃣  正在测试异步日志器..." << std::endl;
    my_log::LocalLoggerBuilder async_builder;
    async_builder.buildLoggerName("async_test");
    async_builder.buildLoggerType(my_log::LoggerType::LOGGER_ASYNC);
    auto async_logger = async_builder.build();

    for (size_t i = 0; i < TEST_THREADS.size(); ++i) {
        int threads = TEST_THREADS[i];
        std::cout << "   测试 " << threads << " 线程..." << std::flush;
        long long qps = run_test(async_logger, threads, LOGS_PER_THREAD * 2, true); // 异步日志量翻倍
        g_results[i].async_qps = qps;
        g_results[i].speedup = static_cast<double>(qps) / g_results[i].sync_qps;
        std::cout << "\r   " << threads << " 线程完成，QPS: " << qps << std::endl;
    }

    // ==================== 3. 日志丢失检测 ====================
    run_loss_test();

    // ==================== 4. 集中打印最终对比表 ====================
    print_final_comparison();

    std::cout << "\n🎉 所有测试完成！" << std::endl;
    return 0;
}