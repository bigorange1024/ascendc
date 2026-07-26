/**
 * @file data_utils.h
 * @brief Alg.7 SampleNTT 探针 Host I/O 工具（在 Huawei 模板基础上仅增本文件头说明）。
 *
 * 本探针用途：
 *   - main.cpp 通过 ReadFile 读 input/seed_d.bin、input/poly_ij.bin
 *   - 核运行后 WriteFile 写 output/{xof,d1,d2,a_hat}.bin
 *   - CHECK_ACL 宏用于 SIM/NPU 路径 ACL 错误检查
 *
 * 与 golden 关系：二进制读写须与 f203_alg7_layout.h 尺寸一致；verify 由 scripts/verify_result.py 完成。
 *
 * 以下 ReadFile/WriteFile/PrintData 等为 Huawei CANN 样例代码，保持原实现不改逻辑。
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

/**
 * PrintData 支持的调试打印数据类型枚举（对齐 ACL aclDataType 取值，本探针仅用 INT32_T/UINT8_T 等少数几种）。
 * 本探针实际只搬运 uint32/uint8/int32（SEED_D、poly(j,i)、xof/d1/d2/a_hat），其余取值为模板保留、未使用。
 */
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

// 统一格式日志宏：INFO/WARN/ERROR 分级前缀，供 main.cpp Host 侧打印诊断信息
#define INFO_LOG(fmt, args...) fprintf(stdout, "[INFO]  " fmt "\n", ##args)
#define WARN_LOG(fmt, args...) fprintf(stdout, "[WARN]  " fmt "\n", ##args)
#define ERROR_LOG(fmt, args...) fprintf(stdout, "[ERROR]  " fmt "\n", ##args)
/**
 * ACL 调用错误检查宏：执行 x（任意返回 aclError 的 ACL 接口调用），
 * 若返回码非 ACL_ERROR_NONE 则打印文件名:行号+错误码，但**不中止程序**（仅打印，无 return/abort）。
 * 用于 main.cpp 中 SIM/NPU（非 __CCE_KT_TEST__）分支的 aclrtMalloc/Memcpy/Stream 等调用包裹。
 */
#define CHECK_ACL(x)                                                                        \
    do {                                                                                    \
        aclError __ret = x;                                                                 \
        if (__ret != ACL_ERROR_NONE) {                                                      \
            std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
        }                                                                                   \
    } while (0);

/**
 * @brief 从二进制文件读取数据到 Host 内存缓冲（Read data from file）。
 * @param [in]  filePath   文件路径（如 input/seed_d.bin）
 * @param [out] fileSize   实际读取的字节数（成功时等于文件大小）
 * @param [out] buffer     调用方预分配的 Host 缓冲，读取内容写入此处
 * @param [in]  bufferSize 缓冲区容量上限（字节）；文件大小超过此值视为失败
 * @return 成功返回 true；文件不存在/非普通文件/为空/超过 bufferSize/打开失败均返回 false
 *
 * 本探针用法：main.cpp 用本函数读 input/seed_d.bin（4B uint32）与 input/poly_ij.bin（2B）。
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

    std::ifstream file;
    file.open(filePath, std::ios::binary);
    if (!file.is_open()) {
        ERROR_LOG("Open file failed. path = %s", filePath.c_str());
        return false;
    }

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
 * @brief 将 Host 内存缓冲写出为二进制文件（Write data to file）。
 * @param [in] filePath 输出文件路径（如 output/a_hat.bin）
 * @param [in] buffer   待写出的 Host 数据指针；为 nullptr 直接失败
 * @param [in] size     写出字节数
 * @return 成功返回 true；buffer 为空/打开失败/实际写入字节数不等于 size 均返回 false
 *
 * 本探针用法：main.cpp 用本函数落盘 output/{xof,d1,d2,a_hat}.bin，供 verify_result.py 对拍。
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
    (void) close(fd);
    if (static_cast<size_t>(writeSize) != size) {
        ERROR_LOG("Write file Failed.");
        return false;
    }

    return true;
}

/**
 * 按固定列宽打印任意算术类型数组（调试用），每行 elementsPerRow 个元素后换行。
 * 本探针未在主链路调用（main.cpp 走 WriteFile 落盘 + Python 对拍），保留供人工调试打印中间量。
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

/** DoPrintData 的 fp16 特化版：先经 aclFloat16ToFloat 转 float 再按列宽打印（本探针未使用 fp16）。 */
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
 * 按 dataType 分发到对应 DoPrintData/DoPrintHalfData 特化实现的统一打印入口（调试用）。
 * @param data          原始字节指针，按 dataType 重新解释
 * @param count         元素个数（非字节数）
 * @param dataType      元素类型标签（见 printDataType 枚举）
 * @param elementsPerRow 每行打印元素个数，默认 16
 * 本探针未在生产路径调用；保留供人工调试时打印 UB/GM dump 数据。
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
