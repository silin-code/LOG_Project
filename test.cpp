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
#include "buffer.hpp"

int main() {
    //读取文件数据，一点一点写入缓冲区，最终将缓冲区数据写入文件，判断生成的新文件是否与源文件一致
    std::ifstream ifs("./logfile/custom.log");
    if (ifs.is_open() == false) return-1;

    ifs.seekg(0, std::ios::end);
    size_t fsize = ifs.tellg();

    ifs.seekg(0, std::ios::beg);
    std::string body;
    body.resize(fsize);
    ifs.read(&body[0], fsize);
    if (ifs.good() == false)
    {
        std::cout << "read error" << std::endl;
    }
    ifs.close();

    my_log::Buffer buffer;
    for(int i=0;i<body.size();i++)
    {
        buffer.push(&body[i],1);
    }

    std::ofstream ofs("./logfile/tmp.log", std::ios::binary);
    ofs.write(buffer.begin(), buffer.readAbleSize());
    for (int i = 0; i < buffer.readAbleSize(); i++)
    {
        ofs.write(buffer.begin(), 1);
        buffer.moveReader(1);
    }
    ofs.close();
    return 0;
}