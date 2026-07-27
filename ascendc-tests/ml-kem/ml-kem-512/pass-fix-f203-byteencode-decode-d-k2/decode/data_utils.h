/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */
/**
 * @file data_utils.h
 * @brief host 侧读写 bin 文件与打印数据的通用工具头（CANN 官方样例模板，非本探针自研算法逻辑）。
 *        在流水线中的位置：main.cpp 用其中的 ReadFile/WriteFile 完成
 *        「input/encoded.bin → host 内存 → device → kernel 计算 → device → host 内存 → output/comp.bin」
 *        的搬运两端；PrintData/DoPrintData 系列为调试打印辅助，本探针未使用但保留以兼容模板。
 *        与 golden 的关系：本文件不参与任何算法计算，只负责按字节读写 gen_data.py/verify_result.py
 *        约定的 .bin 文件，不改变数据内容与语义。
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
#define CHECK_ACL(x)                                                                        \
    do {                                                                                    \
        aclError __ret = x;                                                                 \
        if (__ret != ACL_ERROR_NONE) {                                                      \
            std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
        }                                                                                   \
    } while (0);

/**
 * @brief Read data from file
 * @param [in] filePath: file path
 * @param [out] fileSize: file size
 * @return read result
 *
 * 中文说明：从 filePath 读取二进制文件内容到 buffer。
 * @param filePath   输入文件路径（如 "./input/encoded.bin"）
 * @param fileSize   [出参] 实际读取的字节数
 * @param buffer     调用方预先分配好的接收缓冲区（host 侧内存，非 GM/device 指针）
 * @param bufferSize buffer 的容量上限；若文件实际大小超过此值则读取失败（防越界写）
 * @return 成功返回 true；文件不存在/非常规文件/打开失败/大小为 0/超出缓冲区容量时返回 false
 */
bool ReadFile(const std::string &filePath, size_t &fileSize, void *buffer, size_t bufferSize)
{
    /* 先用 stat 校验路径存在且为常规文件，避免对目录/设备文件等做后续 ifstream 操作。 */
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

    std::ifstream file;
    file.open(filePath, std::ios::binary);
    if (!file.is_open()) {
        ERROR_LOG("Open file failed. path = %s", filePath.c_str());
        return false;
    }

    /* 通过 filebuf 定位到文件末尾获取文件大小，再校验是否超过调用方缓冲区容量，
     * 最后 seek 回文件头一次性读入，避免逐块读取的额外开销。 */
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
    buf->pubseekpos(0, std::ios::in);
    buf->sgetn(static_cast<char *>(buffer), size);
    fileSize = size;
    file.close();
    return true;
}

/**
 * @brief Write data to file
 * @param [in] filePath: file path
 * @param [in] buffer: data to write to file
 * @param [in] size: size to write
 * @return write result
 *
 * 中文说明：把 host 内存 buffer 中的 size 字节写出到 filePath（覆盖已有文件）。
 * @param filePath 输出文件路径（如 "./output/comp.bin"）
 * @param buffer   待写出的 host 侧内存指针（非 GM/device 指针）
 * @param size     待写出的字节数
 * @return 成功返回 true；buffer 为空/打开失败/实际写入字节数与 size 不符时返回 false
 */
bool WriteFile(const std::string &filePath, const void *buffer, size_t size)
{
    if (buffer == nullptr) {
        ERROR_LOG("Write file failed. buffer is nullptr");
        return false;
    }

    /* O_TRUNC：若文件已存在则清空重写，保证每次运行的 output 输出文件都是本次结果。 */
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
 * 中文说明：按 elementsPerRow 个元素一行的格式打印任意数值类型数组（调试辅助，本探针未调用）。
 * @param data           待打印数组首地址（host 侧内存）
 * @param count          元素个数
 * @param elementsPerRow 每行打印的元素个数，用于换行
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

/** 中文说明：DoPrintData 的 fp16 特化版本，打印前先转换为 float（调试辅助，本探针未调用）。 */
void DoPrintHalfData(const aclFloat16 *data, size_t count, size_t elementsPerRow)
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
 * 中文说明：按 dataType 分发到对应类型的 DoPrintData/DoPrintHalfData（调试辅助，本探针未调用）。
 * @param data           待打印数据首地址
 * @param count          元素个数
 * @param dataType       元素的数据类型枚举（见上方 printDataType）
 * @param elementsPerRow 每行打印的元素个数，默认 16
 */
void PrintData(const void *data, size_t count, printDataType dataType, size_t elementsPerRow=16)
{
    if (data == nullptr) {
        ERROR_LOG("Print data failed. data is nullptr");
        return;
    }

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
