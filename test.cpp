// 标准库头文件
#include <iostream>
#include <string>
// 日志模块头文件
#include "level.hpp"
#include "message.hpp"
#include "format.hpp"
#include "sink.hpp"
#include "logger.hpp"
#include "util.hpp"


int main() {
    std::cout << "===== 建造者模式日志器测试 =====\n\n";

    // ===================== 测试1：最简配置 =====================
    std::cout << "【测试1：最简配置】\n";
    my_log::LocalLoggerBuilder builder1;
    builder1.buildLoggerName("simple_logger"); // 用你原来的buildLoggerName
    my_log::Logger::ptr logger1 = builder1.build();

    logger1->debug(__FILE__, __LINE__, "这是DEBUG日志");
    logger1->info(__FILE__, __LINE__, "这是INFO日志");
    logger1->warn(__FILE__, __LINE__, "这是WARN日志");

    // ===================== 测试2：完整自定义配置 =====================
    std::cout << "\n【测试2：完整自定义配置】\n";
    my_log::LocalLoggerBuilder builder2;
    builder2.buildLoggerName("custom_logger");
    builder2.buildLoggerLevel(my_log::LogLevel::value::WARN); // 只输出WARN及以上
    builder2.buildFormatter("[%d{%Y-%m-%d %H:%M:%S}][%p][%c] %m%n");
    builder2.buildSink<my_log::StdoutSink>(); // 控制台输出
    builder2.buildSink<my_log::FileSink>("./logfile/custom.log"); // 文件输出
    builder2.buildSink<my_log::RollSizeSink>("./logfile/custom_roll_", 1024 * 1024); // 滚动文件
    my_log::Logger::ptr logger2 = builder2.build();

    // 等级过滤：DEBUG/INFO会被丢弃
    logger2->debug(__FILE__, __LINE__, "DEBUG日志（应该被过滤）");
    logger2->info(__FILE__, __LINE__, "INFO日志（应该被过滤）");
    // WARN及以上正常输出
    logger2->warn(__FILE__, __LINE__, "WARN日志（正常输出）");
    logger2->error(__FILE__, __LINE__, "ERROR日志（正常输出）");
    logger2->fatal(__FILE__, __LINE__, "FATAL日志（正常输出）");

    std::cout << "\n===== 所有测试通过！建造者模式工作正常 =====\n";
    return 0;
}