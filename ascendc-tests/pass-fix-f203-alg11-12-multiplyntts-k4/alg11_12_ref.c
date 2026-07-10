/**
 * 【文件头】Alg.11 MultiplyNTTs 的 C 导出包装（供 gen_data.py ctypes）。
 *
 * 本文件在流水线中的位置：host golden 生成链；算法本体在 alg11_12_ref.h 内联。
 * 作用：编译为共享库后暴露 alg11_multiply_ntts，与 Python 参考对拍。
 * 与 golden 关系：gen_data 用本符号与 Python 路径交叉验证后写出 golden_h.bin。
 */
#include "alg11_12_ref.h"

/**
 * 导出入口：直接转发到内联 Alg.11。
 * @param h  输出 [256] int32
 * @param f  输入左 poly [256] int32
 * @param g  输入右 poly [256] int32
 */
void alg11_multiply_ntts(int32_t *h, const int32_t *f, const int32_t *g)
{
    alg11_multiply_ntts_inline(h, f, g);
}
