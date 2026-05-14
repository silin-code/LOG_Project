/*
工具类的实现
    1获取系统时间
    2判断文件是否存在
    3获取文件所在路径
    4创建目录
*/
#pragma once
#include <iostream>
#include <ctime>
#include <string>


/*跨平台兼容*/
#ifdef _WIN32
//windows
#include <direct.h>
#include <windows.h>
#define stat _stat//兼容windows的_stat
#define mkdir _mkdir//兼容windows的_mkdir
#else
//linux
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace my_log
{
    namespace util
    {
        class Date
        {
        public:
            static time_t GetTime()//Get Systemtime
            {
                return time(nullptr);//平台无差异
            }
        };

        class File
        {
        public:
            static bool Exists(const std::string &pathname)//judje file exists
            {
                //需要跨平台兼容
                struct stat st;
                return stat(pathname.c_str(),&st) == 0;
            }

            static std::string Path(const std::string &pathname)//get path of file
            {
                size_t pos = pathname.find_last_of("/\\");//找到最后一个/或者\\的位置
                if (pos == std::string::npos)
                    return ".";//无路径返回当前目录
                return pathname.substr(0, pos);
            }
            static void createdirectory(const std::string &pathname)//create directory
            {
                if (pathname.empty() || Exists(pathname)) return;

                std::string current_path;
                size_t idx = 0;
                size_t len = pathname.size();
                while (idx < len)
                {
                    //查看路径分隔符
                    size_t pos = pathname.find_first_of("/\\", idx);
                    if (pos == std::string::npos)
                    {
                        pos = len;
                    }
                    //截取子路径
                    current_path = pathname.substr(0, pos);
                    if (!current_path.empty() && !Exists(current_path))
                    {
#ifdef _WIN32
                        int n=mkdir(current_path.c_str());//windows don't need control
                        (void)n;
#else
                        mkdir(current_path.c_str(), 0777);
#endif
                    }
                    idx = pos + 1;
                }
            }
        };
    }
}