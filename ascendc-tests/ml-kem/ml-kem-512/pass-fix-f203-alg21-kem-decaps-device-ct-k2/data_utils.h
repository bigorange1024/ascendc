/**
 * @file data_utils.h
 * @brief Host 侧 bin 读写、ACL 错误检查宏与调试打印（Decaps / Encrypt 用例共用）。
 *
 * ## 流水线位置
 * FIPS 203 ML-KEM Decaps（Alg.21）或 Encrypt（Alg.14）的 **host 编排层**（非设备核）。
 * 负责把 input/ 下各 .bin 装入 host 缓冲、把设备回写结果落到 output/（如 K.bin、c.bin）。
 *
 * ## 与 golden 关系
 * 仅 I/O 胶水：不参与密码学计算；对拍由 `run.sh` + `verify_kem_decaps.py` 或 `cmp` 完成。
 *
 * ## 本文件提供
 * - `ReadFile` / `WriteFile`：定长缓冲的二进制读写
 * - `CHECK_ACL`：ACL API 失败时打印文件行号
 * - `PrintData`：按 dtype 调试打印（可选）
 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */
#ifndef DATA_UTILS_H
#define DATA_UTILS_H
#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>
#include <vector>
#include <iomanip>
#include <cassert>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "acl/acl.h"

/** 调试打印时的 dtype 分派枚举（与 ACL 张量类型 loosely 对应） */
typedef enum {
    DT_UNDEFINED = -1,
    FLOAT = 0,
    HALF = 1,
    INT8_T = 2,
    INT32_T = 3,
    UINT8_T = 4,
    INT16_T = 6,
    UINT16_T = 7,
    UINT32_T = 8,
    INT64_T = 9,
    UINT64_T = 10,
    DOUBLE = 11,
    BOOL = 12,
    STRING = 13,
    COMPLEX64 = 16,
    COMPLEX128 = 17,
    BF16 = 27
} printDataType;

#define INFO_LOG(fmt, args...) fprintf(stdout, "[INFO]  " fmt "\n", ##args)
#define WARN_LOG(fmt, args...) fprintf(stdout, "[WARN]  " fmt "\n", ##args)
#define ERROR_LOG(fmt, args...) fprintf(stdout, "[ERROR]  " fmt "\n", ##args)
/** ACL 调用包装：失败时打印源位置与 aclError 码（不 abort） */
#define CHECK_ACL(x)                                                                        \
    do {                                                                                    \
        aclError __ret = x;                                                                 \
        if (__ret != ACL_ERROR_NONE) {                                                      \
            std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
        }                                                                                   \
    } while (0);

/**
 * 从路径读取整个常规文件到 host 缓冲。
 * @param filePath 输入 bin 路径（如 input/dk_kem.bin、input/c.bin）
 * @param fileSize [out] 实际读入字节数
 * @param buffer 调用方预分配缓冲（尺寸须 ≥ 文件）
 * @param bufferSize 缓冲容量；文件更大则失败
 * @return true 成功；false 路径非法 / 空文件 / 溢出
 */
inline bool ReadFile(const std::string &filePath, size_t &fileSize, void *buffer, size_t bufferSize)
{
    // 1) 确认路径存在且为常规文件（拒绝目录等）
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

    // 2) 以二进制打开
    std::ifstream file;
    file.open(filePath, std::ios::binary);
    if (!file.is_open()) {
        ERROR_LOG("Open file failed. path = %s", filePath.c_str());
        return false;
    }

    // 3) 测长：空文件或超过 bufferSize 均拒绝，避免半读
    std::filebuf *buf = file.rdbuf();
    size_t size = buf->pubseekoff(0, std::ios::end, std::ios::in);
    if (size == 0) {
        ERROR_LOG("file size is 0");
        file.close();
        return false;
    }
    if (size > bufferSize) {
        ERROR_LOG("file size is larger than buffer size");
        file.close();
        return false;
    }
    // 4) 回卷并一次性读入
    buf->pubseekpos(0, std::ios::in);
    buf->sgetn(static_cast<char *>(buffer), size);
    fileSize = size;
    file.close();
    return true;
}

/**
 * 将 host 缓冲整段写入路径（覆盖创建）。
 * @param filePath 输出路径（如 output/K.bin、output/c.bin）
 * @param buffer 待写数据；不得为空
 * @param size 字节数；须与 write 返回值一致
 * @return true 成功
 */
inline bool WriteFile(const std::string &filePath, const void *buffer, size_t size)
{
    if (buffer == nullptr) {
        ERROR_LOG("Write file failed. buffer is nullptr");
        return false;
    }

    // O_TRUNC：每次覆盖，避免残留旧 golden 尾部
    int fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWRITE);
    if (fd < 0) {
        ERROR_LOG("Open file failed. path = %s", filePath.c_str());
        return false;
    }

    auto writeSize = write(fd, buffer, size);
    (void) close(fd);
    if (static_cast<size_t>(writeSize) != size) {
        ERROR_LOG("Write file Failed.");
        return false;
    }

    return true;
}

/**
 * 按行打印整型/浮点数组（调试用，非验收路径）。
 * @param data 元素指针；@param count 元素个数；@param elementsPerRow 每行个数
 */
template<typename T>
void DoPrintData(const T *data, size_t count, size_t elementsPerRow)
{
    assert(elementsPerRow != 0);
    for (size_t i = 0; i < count; ++i) {
        std::cout << std::setw(10) << data[i];
        if (i % elementsPerRow == elementsPerRow - 1) {
            std::cout << std::endl;
        }
    }
}

/** 打印 aclFloat16：先转 float 再输出。 */
inline void DoPrintHalfData(const aclFloat16 *data, size_t count, size_t elementsPerRow)
{
    assert(elementsPerRow != 0);
    for (size_t i = 0; i < count; ++i) {
        std::cout << std::setw(10) << std::setprecision(6) << aclFloat16ToFloat(data[i]);
        if (i % elementsPerRow == elementsPerRow - 1) {
            std::cout << std::endl;
        }
    }
}

/**
 * 按 printDataType 分派调试打印。
 * @param data 无类型缓冲；@param count 元素个数；@param dataType 解释类型；@param elementsPerRow 默认 16
 */
inline void PrintData(const void *data, size_t count, printDataType dataType, size_t elementsPerRow=16)
{
    if (data == nullptr) {
        ERROR_LOG("Print data failed. data is nullptr");
        return;
    }

    // 按 dtype 选择打印实现；未覆盖类型报错
    switch (dataType) {
        case BOOL:
            DoPrintData(reinterpret_cast<const bool *>(data), count, elementsPerRow);
            break;
        case INT8_T:
            DoPrintData(reinterpret_cast<const int8_t *>(data), count, elementsPerRow);
            break;
        case UINT8_T:
            DoPrintData(reinterpret_cast<const uint8_t *>(data), count, elementsPerRow);
            break;
        case INT16_T:
            DoPrintData(reinterpret_cast<const int16_t *>(data), count, elementsPerRow);
            break;
        case UINT16_T:
            DoPrintData(reinterpret_cast<const uint16_t *>(data), count, elementsPerRow);
            break;
        case INT32_T:
            DoPrintData(reinterpret_cast<const int32_t *>(data), count, elementsPerRow);
            break;
        case UINT32_T:
            DoPrintData(reinterpret_cast<const uint32_t *>(data), count, elementsPerRow);
            break;
        case INT64_T:
            DoPrintData(reinterpret_cast<const int64_t *>(data), count, elementsPerRow);
            break;
        case UINT64_T:
            DoPrintData(reinterpret_cast<const uint64_t *>(data), count, elementsPerRow);
            break;
        case HALF:
            DoPrintHalfData(reinterpret_cast<const aclFloat16 *>(data), count, elementsPerRow);
            break;
        case FLOAT:
            DoPrintData(reinterpret_cast<const float *>(data), count, elementsPerRow);
            break;
        case DOUBLE:
            DoPrintData(reinterpret_cast<const double *>(data), count, elementsPerRow);
            break;
        default:
            ERROR_LOG("Unsupported type: %d", dataType);
    }
    std::cout << std::endl;
}
#endif // DATA_UTILS_H
