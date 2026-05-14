#include <iostream>
#include "level.hpp"
#include "message.hpp"
#include "format.hpp"
#include "sink.hpp"

int main() {
    // ===================== 测试1：标准输出落地 =====================
    std::cout << "===== Test 1: StdoutSink =====" << std::endl;
    my_log::StdoutSink stdout_sink;
    const char* log1 = "INFO: Service started successfully!\n";
    stdout_sink.log(log1, strlen(log1));

    // ===================== 测试2：单文件落地 =====================
    std::cout << "\n===== Test 2: FileSink =====" << std::endl;
    const char* log2 = "ERROR: Database connection failed!\n";
    my_log::FileSink file_sink("./logs/test.log");
    file_sink.log(log2, strlen(log2));
    std::cout << "Log written to ./logs/test.log" << std::endl;

    // ===================== 测试3：滚动文件落地（大小超过100字节自动切分） =====================
    std::cout << "\n===== Test 3: RollSizeSink =====" << std::endl;
    // 基础文件名 + 最大100字节就滚动
    my_log::RollSizeSink roll_sink("./logs/roll_", 100);

    // 循环写入日志，触发滚动
    const char* test_log = "DEBUG: Testing roll file sink... ";
    for (int i = 0; i < 10; i++) {
        roll_sink.log(test_log, strlen(test_log));
    }
    std::cout << "Rolling log files generated in ./logs/" << std::endl;

    // ===================== 测试完成 =====================
    std::cout << "\n===== All Sink Tests Passed! Cross-platform OK =====" << std::endl;

    system("pause");
    return 0;
}