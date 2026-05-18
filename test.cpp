#include <iostream>
#include "level.hpp"
#include "message.hpp"
#include "format.hpp"
#include "sink.hpp"
{int main() {
    // 1. 创建标准输出落地
    auto stdout_sink = my_log::SinkFactory::create<my_log::StdoutSink>();

    // 2. 创建单文件落地
    auto file_sink = my_log::SinkFactory::create<my_log::FileSink>("./logs/test.log");

    // 3. 创建滚动文件落地（1MB大小滚动）
    auto roll_sink = my_log::SinkFactory::create<my_log::RollSizeSink>("./logs/roll_", 1024 * 1024);

    // 4. 统一调用log接口
    const char* msg = "INFO: Factory pattern test success!\n";
    stdout_sink->log(msg, strlen(msg));
    file_sink->log(msg, strlen(msg));
    roll_sink->log(msg, strlen(msg)); 
    return 0;
}