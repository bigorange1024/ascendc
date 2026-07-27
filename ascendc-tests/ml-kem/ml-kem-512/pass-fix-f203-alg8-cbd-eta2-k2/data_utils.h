/*
 * @file data_utils.h
 * @brief Host 侧文件 I/O 与 ACL 错误打印工具。
 *
 * 本文件是 KernelLaunch 探针的基础设施：`main.cpp` 用 `ReadFile` 读取
 * `input/prf_out.bin`，用 `WriteFile` 写出 `output/src.bin`，SIM/NPU 分支用
 * `CHECK_ACL` 打印 ACL 调用错误。它不参与 CBD 数学计算；golden 对拍由脚本完成。
 */
#ifndef DATA_UTILS_H
#define DATA_UTILS_H

#include "acl/acl.h"

#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#define ERROR_LOG(fmt, args...) fprintf(stdout, "[ERROR]  " fmt "\n", ##args)
#define CHECK_ACL(x)                                                                        \
    do {                                                                                    \
        aclError __ret = x;                                                                 \
        if (__ret != ACL_ERROR_NONE) {                                                      \
            std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
        }                                                                                   \
    } while (0)

/**
 * 读取二进制文件到调用者提供的 buffer。
 * @param filePath 输入文件路径
 * @param fileSize 实际读取到的字节数
 * @param buffer 目标缓冲区
 * @param bufferSize 目标缓冲区容量
 * @return true=读取成功；false=路径异常、非普通文件、空文件或容量不足
 */
bool ReadFile(const std::string &filePath, size_t &fileSize, void *buffer, size_t bufferSize)
{
    struct stat sBuf;
    int fileStatus = stat(filePath.data(), &sBuf);
    if (fileStatus == -1) {
        ERROR_LOG("failed to get file");
        return false;
    }
    if (S_ISREG(sBuf.st_mode) == 0) {
        ERROR_LOG("%s is not a file, please enter a file", filePath.c_str());
        return false;
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        ERROR_LOG("Open file failed. path = %s", filePath.c_str());
        return false;
    }

    /* 先定位末尾获取文件大小，再回到开头一次性读入，避免逐字节 I/O。 */
    std::filebuf *buf = file.rdbuf();
    size_t size = buf->pubseekoff(0, std::ios::end, std::ios::in);
    if (size == 0) {
        ERROR_LOG("file size is 0");
        return false;
    }
    if (size > bufferSize) {
        ERROR_LOG("file size is larger than buffer size");
        return false;
    }
    buf->pubseekpos(0, std::ios::in);
    buf->sgetn(static_cast<char *>(buffer), size);
    fileSize = size;
    return true;
}

/**
 * 以二进制方式写入文件；存在则截断重写。
 * @param filePath 输出文件路径
 * @param buffer 待写缓冲区
 * @param size 写入字节数
 */
bool WriteFile(const std::string &filePath, const void *buffer, size_t size)
{
    if (buffer == nullptr) {
        ERROR_LOG("Write file failed. buffer is nullptr");
        return false;
    }

    int fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWRITE);
    if (fd < 0) {
        ERROR_LOG("Open file failed. path = %s", filePath.c_str());
        return false;
    }

    auto writeSize = write(fd, buffer, size);
    (void)close(fd);
    if (static_cast<size_t>(writeSize) != size) {
        ERROR_LOG("Write file Failed.");
        return false;
    }
    return true;
}

#endif  // DATA_UTILS_H
