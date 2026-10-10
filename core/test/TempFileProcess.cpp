/**
 * @file TempFileProcess.cpp
 * @brief 临时目录回归测试子进程：通过标准输入协调正常退出和文件占用。
 */
#include "TempFile.h"

#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[])
{
    try {
        // 比 TempFile 单例更早构造，保证其析构时文件仍被占用。
        static std::ofstream held_file;
        const auto path = core::TempFile::instance().path();
        {
            std::ofstream file(path);
            file.exceptions(std::ios::failbit | std::ios::badbit);
            file << "alive";
        }
        if (argc > 1 && std::string(argv[1]) == "locked") {
            held_file.open(path, std::ios::app);
            held_file.exceptions(std::ios::failbit | std::ios::badbit);
#ifdef _WIN32
            std::error_code error;
            if (std::filesystem::remove(path, error) || !error)
                throw std::runtime_error("Expected the open file to reject deletion");
#endif
        }
        const auto encoded = path.u8string();
        std::cout.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
        std::cout << std::endl;

        std::string command;
        while (std::getline(std::cin, command)) {
            if (command == "exit")
                return 0;
            if (command == "verify") {
                if (!std::filesystem::exists(path)) {
                    std::cout << "missing" << std::endl;
                    continue;
                }
                std::ofstream file(path, std::ios::app);
                file << "still writable";
                std::cout << (file ? "ok" : "failed") << std::endl;
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
