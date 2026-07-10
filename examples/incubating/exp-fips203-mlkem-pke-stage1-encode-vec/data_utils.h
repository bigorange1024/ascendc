/**
 * @file data_utils.h
 * @brief Stage1 encode 探针 Host I/O 工具（Huawei 样例 ReadFile/WriteFile）。
 *
 * 流水线位置：main.cpp 读 input/se_polyvec_gm.bin、写 output/mat_a_gm.bin。
 * 对齐：F203 Stage1 limb6 编码；与 golden 仅 I/O 等价（scripts 对拍）。
 * 以下实现保持 CANN 样例逻辑，本文件仅补充中文说明。
 */

#ifndef DATA_UTILS_H
#define DATA_UTILS_H
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "acl/acl.h"

#define ERROR_LOG(fmt, args...) fprintf(stdout, "[ERROR]  " fmt "\n", ##args)
#define CHECK_ACL(x)                                                                        \
    do {                                                                                    \
        aclError __ret = x;                                                                 \
        if (__ret != ACL_ERROR_NONE) {                                                      \
            std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
        }                                                                                   \
    } while (0);

/**
 * 从磁盘读二进制到 Host buffer。
 * @param filePath 路径；@param fileSize 输出实际字节；@param buffer 目标；@param bufferSize 容量上限
 * @return 成功 true；文件过大/空/打开失败则 false
 */
bool ReadFile(const std::string &filePath, size_t &fileSize, void *buffer, size_t bufferSize)
{
    // 先 stat 确认路径存在
    struct stat sBuf;
    if (stat(filePath.data(), &sBuf) == -1) {
        ERROR_LOG("failed to get file");
        return false;
    }
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        ERROR_LOG("Open file failed. path = %s", filePath.c_str());
        return false;
    }
    // 测文件长度；超过 bufferSize 则拒绝，避免越界
    std::filebuf *buf = file.rdbuf();
    size_t size = buf->pubseekoff(0, std::ios::end, std::ios::in);
    if (size == 0 || size > bufferSize) {
        file.close();
        return false;
    }
    buf->pubseekpos(0, std::ios::in);
    buf->sgetn(static_cast<char *>(buffer), size);
    fileSize = size;
    file.close();
    return true;
}

/**
 * 将 Host buffer 写为二进制文件（覆盖创建）。
 * @param filePath 路径；@param buffer 源；@param size 字节数
 */
bool WriteFile(const std::string &filePath, const void *buffer, size_t size)
{
    if (buffer == nullptr) {
        return false;
    }
    int fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWRITE);
    if (fd < 0) {
        return false;
    }
    size_t writeSize = write(fd, buffer, size);
    (void)close(fd);
    return writeSize == size;
}

#endif
