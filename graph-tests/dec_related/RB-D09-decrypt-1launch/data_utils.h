/**
 * @file data_utils.h
 * @brief RB-D09 Host 侧文件 I/O 与 ACL 检查宏（工程壳；无算法逻辑）。
 */
#ifndef RB_D09_DATA_UTILS_H
#define RB_D09_DATA_UTILS_H

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "acl/acl.h"

#define INFO_LOG(fmt, args...) fprintf(stdout, "[INFO]  " fmt "\n", ##args)
#define ERROR_LOG(fmt, args...) fprintf(stdout, "[ERROR]  " fmt "\n", ##args)

#define CHECK_ACL(x)                                                                        \
    do {                                                                                    \
        aclError __ret = x;                                                                 \
        if (__ret != ACL_ERROR_NONE) {                                                      \
            std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
        }                                                                                   \
    } while (0)

/**
 * 读二进制文件到 buffer。
 * @param filePath 路径
 * @param fileSize [out] 实际读入字节
 * @param buffer 目标缓冲
 * @param bufferSize 缓冲容量；文件更大则失败
 */
inline bool ReadFile(const std::string &filePath, size_t &fileSize, void *buffer, size_t bufferSize)
{
    struct stat sBuf;
    if (stat(filePath.data(), &sBuf) == -1) {
        ERROR_LOG("failed to get file %s", filePath.c_str());
        return false;
    }
    if (S_ISREG(sBuf.st_mode) == 0) {
        ERROR_LOG("%s is not a regular file", filePath.c_str());
    }
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        ERROR_LOG("Open file failed. path = %s", filePath.c_str());
        return false;
    }
    std::filebuf *buf = file.rdbuf();
    size_t size = buf->pubseekoff(0, std::ios::end, std::ios::in);
    if (size == 0 || size > bufferSize) {
        ERROR_LOG("bad file size %zu (cap %zu)", size, bufferSize);
        return false;
    }
    buf->pubseekpos(0, std::ios::in);
    buf->sgetn(static_cast<char *>(buffer), size);
    fileSize = size;
    return true;
}

/**
 * 写二进制文件。
 * @param filePath 路径
 * @param buffer 源
 * @param size 字节数
 */
inline bool WriteFile(const std::string &filePath, const void *buffer, size_t size)
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
    size_t writeSize = static_cast<size_t>(write(fd, buffer, size));
    (void)close(fd);
    return writeSize == size;
}

#endif
