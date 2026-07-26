/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 * @file data_utils.h
 * @brief 通用 Host 侧文件 I/O 与调试打印工具（各探针 main.cpp 共用的样板代码）。
 *
 * 本文件在流水线中的位置：不属于本探针（pass-fix-f203-alg8-cbd-eta2-k3）算法逻辑本身，
 * 而是 Host 主程序 main.cpp 依赖的基础设施：读写 input/output 下的二进制文件
 * （ReadFile/WriteFile）、按类型打印张量数据（PrintData 系列，调试用）、以及
 * ACL 错误检查宏 CHECK_ACL。与 golden 无直接关系，仅为落盘/读盘提供通用能力。
 * 本文件在各探针目录下均有一份几乎相同的拷贝（历史沿用的 Huawei 官方样例代码），
 * 此处按仓库规范补充中文说明，不改动任何函数签名/逻辑。
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

/* PrintData 系列函数使用的数据类型标签，取值与 acl 内部 dtype 编码保持一致，
 * 便于以统一入口打印不同 dtype 的张量内容（调试用，不参与正式对拍逻辑）。 */
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

/* 统一日志宏：INFO/WARN/ERROR 三级，均输出到 stdout（含来源标签），便于 run.sh 日志抓取。 */
#define INFO_LOG(fmt, args...) fprintf(stdout, "[INFO]  " fmt "\n", ##args)
#define WARN_LOG(fmt, args...) fprintf(stdout, "[WARN]  " fmt "\n", ##args)
#define ERROR_LOG(fmt, args...) fprintf(stdout, "[ERROR]  " fmt "\n", ##args)
/* ACL 调用错误检查：执行表达式 x（应返回 aclError），非 ACL_ERROR_NONE 时打印文件名+行号+错误码。
 * 注意：仅打印不中断执行（无 return/abort），调用侧若需要感知失败须自行检查返回值。 */
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
 * @param filePath  [in]  文件路径（相对/绝对均可）
 * @param fileSize  [out] 实际读取到的字节数（等于文件大小）
 * @param buffer    [out] 调用者分配的接收缓冲区
 * @param bufferSize [in] buffer 容量上限，文件大小超过该值会读取失败（防止越界写）
 * @return true=读取成功；false=文件不存在/非普通文件/打开失败/文件为空/超出缓冲区容量
 */
bool ReadFile(const std::string &filePath, size_t &fileSize, void *buffer, size_t bufferSize)
{
    /* 先用 stat 校验路径存在且是普通文件（排除目录/设备文件等），再打开做二进制读取 */
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

    /* 用 filebuf 的 seek 定位到文件末尾获取文件大小，再回到起始位置一次性整块读入 */
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
 * 中文说明：把内存缓冲区 buffer 的前 size 字节以二进制方式写入 filePath（存在则截断重写）。
 * @param filePath [in] 目标文件路径
 * @param buffer   [in] 待写入数据指针，不可为空
 * @param size     [in] 待写入字节数
 * @return true=写入成功且写入字节数与 size 相符；false=buffer 为空/打开失败/写入字节数不符
 */
bool WriteFile(const std::string &filePath, const void *buffer, size_t size)
{
    if (buffer == nullptr) {
        ERROR_LOG("Write file failed. buffer is nullptr");
        return false;
    }

    /* O_TRUNC：若文件已存在则清空重写；S_IRUSR|S_IWRITE：新建文件时的权限位（用户读写） */
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
 * 按给定元素类型 T，逐元素打印数据，每行打印 elementsPerRow 个后换行（纯调试输出）。
 * @param data 指向 count 个 T 类型元素的数组
 * @param count 元素总数
 * @param elementsPerRow 每行打印的元素个数（不能为 0，否则触发断言）
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

/** fp16（aclFloat16）专用打印：先转换为 float 再按 6 位精度输出（fp16 无法直接用 << 打印）。 */
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
 * 按 dataType 分发到对应的 DoPrintData/DoPrintHalfData 实例化，统一调试打印入口。
 * @param data 数据缓冲区指针（按 dataType 解释为对应 C 类型数组）
 * @param count 元素个数
 * @param dataType 数据类型标签（printDataType 枚举）
 * @param elementsPerRow 每行打印元素数，默认 16
 */
void PrintData(const void *data, size_t count, printDataType dataType, size_t elementsPerRow=16)
{
    if (data == nullptr) {
        ERROR_LOG("Print data failed. data is nullptr");
        return;
    }

    /* 按枚举值分发到具体类型的打印实现；未覆盖的类型（如 STRING/COMPLEX/BF16）走 default 报错 */
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
