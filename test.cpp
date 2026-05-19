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
    // ========== 1. 日志初始化配置 ==========
    std::string logger_name = "syclogger";       // 日志器名称
    // 日志等级：WARN及以上级别才会输出（DEBUG/INFO会被过滤）
    my_log::LogLevel::value limit = my_log::LogLevel::value::WARN;
    // 修复：日志格式字符串（修正语法错误，规范格式）
    my_log::Formatter::ptr fmt(new my_log::Formatter("[%d{%H:%M:%S}][%p][%f:%l] %m%n"));

    // ========== 2. 创建三种日志落地器 ==========
    // 控制台输出
    auto stout_lsp = my_log::SinkFactory::create<my_log::StdoutSink>();
    // 单个文件输出
    auto file_lsp = my_log::SinkFactory::create<my_log::FileSink>("./logfile/test.log");
    // 按大小滚动文件（1MB 滚动一次）
    auto roll_lsp = my_log::SinkFactory::create<my_log::RollSizeSink>("./logfile/roll-", 1024 * 1024);

    // ========== 3. 组装落地器数组 ==========
    std::vector<my_log::LogSink::ptr> sinks = { stout_lsp ,file_lsp, roll_lsp };

    // ========== 4. 创建同步日志器 ==========
    my_log::Logger::ptr logger(new my_log::SyncLogger(logger_name, limit, fmt, sinks));

    // ========== 5. 测试日志等级过滤 ==========
    // 等级低于WARN，不会输出
    logger->debug(__FILE__, __LINE__, "测试日志");
    logger->info(__FILE__, __LINE__, "测试日志");
    // 等级≥WARN，正常输出
    logger->warn(__FILE__, __LINE__, "测试日志");
    logger->error(__FILE__, __LINE__, "测试日志");
    logger->fatal(__FILE__, __LINE__, "测试日志");

    // ========== 6. 测试【按大小滚动文件】功能 ==========
    size_t cur_size = 0;    // 当前写入数据大小
    size_t cur_count = 0;   // 日志计数
    std::string str = "测试日志-";
    // 循环写入日志，直到总大小超过 1MB，触发文件滚动
    while (cur_size < 1024 * 1024)
    {
        // 拼接日志内容
        std::string tmp = str + std::to_string(cur_count++);
        // 修复：补全缺失的格式化参数，FATAL级别强制输出
        logger->fatal(__FILE__, __LINE__, "%s", tmp.c_str());
        // 累加预估大小（控制循环次数）
        cur_size += 200;
    }

    return 0;
}