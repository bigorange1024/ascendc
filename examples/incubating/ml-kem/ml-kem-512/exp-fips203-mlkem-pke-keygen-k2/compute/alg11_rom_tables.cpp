// @probe exp-fips203-mlkem-pke-keygen-k2
// @file compute/alg11_rom_tables.cpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `alg11_rom_tables.cpp` 为该子模块组件。 / Component: alg11_rom_tables.cpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: alg11_rom_tables.h, kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 行 18 Alg.11 basemul / γ 表与向量管线。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-768（k=3）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/alg11_rom_tables.cpp
 */
/**
 * @file alg11_rom_tables.cpp
 * @brief Alg.11 设备 GM 常量 ROM 定义（γ、Gather 字节索引、interleave 重排表）。
 *
 * 链接：MIX 核 AIC/AIV 各编一份；CPU 调试仅 AIV + host 链接一次（见 mmad_custom.cpp 条件 include）。
 * Init 阶段由 alg11_vec::init_rom_luts_ub DataCopy 进 UB；Compute 热路径禁止 SetValue 重填。
 *
 * 表内容：
 *   - gAlg11GammasGm           — FIPS kMlkemGammas[128]，与 alg11_gammas.h 宏表一致
 *   - gAlg11GatherEven/OddByte — 偶/奇系数 Gather 字节偏移（n=256 固定公式）
 *   - gAlg11InterleaveReorder  — basemul 后 c0||c1 → 交错 h[256] 的索引
 *
 * 同步：hat_inner_product_ref.c / hat_gammas.hpp 中 γ 须逐元一致。
 */
#include "alg11_rom_tables.h"
#include "kernel_operator.h"

#define ALG11_GATHER_EVEN_BYTE_TABLE                                                                                  \
    0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144, 152, 160, 168, 176, 184, 192, \
        200, 208, 216, 224, 232, 240, 248, 256, 264, 272, 280, 288, 296, 304, 312, 320, 328, 336, 344, 352, 360,    \
        368, 376, 384, 392, 400, 408, 416, 424, 432, 440, 448, 456, 464, 472, 480, 488, 496, 504, 512, 520, 528,  \
        536, 544, 552, 560, 568, 576, 584, 592, 600, 608, 616, 624, 632, 640, 648, 656, 664, 672, 680, 688, 696,  \
        704, 712, 720, 728, 736, 744, 752, 760, 768, 776, 784, 792, 800, 808, 816, 824, 832, 840, 848, 856, 864,  \
        872, 880, 888, 896, 904, 912, 920, 928, 936, 944, 952, 960, 968, 976, 984, 992, 1000, 1008, 1016

#define ALG11_GATHER_ODD_BYTE_TABLE                                                                                   \
    4, 12, 20, 28, 36, 44, 52, 60, 68, 76, 84, 92, 100, 108, 116, 124, 132, 140, 148, 156, 164, 172, 180, 188, 196,  \
        204, 212, 220, 228, 236, 244, 252, 260, 268, 276, 284, 292, 300, 308, 316, 324, 332, 340, 348, 356, 364,  \
        372, 380, 388, 396, 404, 412, 420, 428, 436, 444, 452, 460, 468, 476, 484, 492, 500, 508, 516, 524, 532,    \
        540, 548, 556, 564, 572, 580, 588, 596, 604, 612, 620, 628, 636, 644, 652, 660, 668, 676, 684, 692, 700,  \
        708, 716, 724, 732, 740, 748, 756, 764, 772, 780, 788, 796, 804, 812, 820, 828, 836, 844, 852, 860, 868,  \
        876, 884, 892, 900, 908, 916, 924, 932, 940, 948, 956, 964, 972, 980, 988, 996, 1004, 1012, 1020

/* interleave: scratch=[c0||c1] 各 128 int32；输出 h[i] 取自 scratch[reorder[i]] */
#define ALG11_INTERLEAVE_REORDER_BYTE_TABLE                                                                           \
    0, 512, 4, 516, 8, 520, 12, 524, 16, 528, 20, 532, 24, 536, 28, 540, 32, 544, 36, 548, 40, 552, 44, 556, 48,    \
        560, 52, 564, 56, 568, 60, 572, 64, 576, 68, 580, 72, 584, 76, 588, 80, 592, 84, 596, 88, 600, 92, 604, 96,  \
        608, 100, 612, 104, 616, 108, 620, 112, 624, 116, 628, 120, 632, 124, 636, 128, 640, 132, 644, 136, 648,    \
        140, 652, 144, 656, 148, 660, 152, 664, 156, 668, 160, 672, 164, 676, 168, 680, 172, 684, 176, 688, 180,   \
        692, 184, 696, 188, 700, 192, 704, 196, 708, 200, 712, 204, 716, 208, 720, 212, 724, 216, 728, 220, 732,    \
        224, 736, 228, 740, 232, 744, 236, 748, 240, 752, 244, 756, 248, 760, 252, 764, 256, 768, 260, 772, 264,   \
        776, 268, 780, 272, 784, 276, 788, 280, 792, 284, 796, 288, 800, 292, 804, 296, 808, 300, 812, 304, 816,    \
        308, 820, 312, 824, 316, 828, 320, 832, 324, 836, 328, 840, 332, 844, 336, 848, 340, 852, 344, 856, 348,  \
        860, 352, 864, 356, 868, 360, 872, 364, 876, 368, 880, 372, 884, 376, 888, 380, 892, 384, 896, 388, 900,   \
        392, 904, 396, 908, 400, 912, 404, 916, 408, 920, 412, 924, 416, 928, 420, 932, 424, 936, 428, 940, 432,  \
        944, 436, 948, 440, 952, 444, 956, 448, 960, 452, 964, 456, 968, 460, 972, 464, 976, 468, 980, 472, 984,    \
        476, 988, 480, 992, 484, 996, 488, 1000, 492, 1004, 496, 1008, 500, 1012, 504, 1016, 508, 1020

__gm__ const int32_t gAlg11GammasGm[ALG11_PAIR_COUNT] = {ALG11_GAMMAS_TABLE};
__gm__ const int32_t gAlg11GatherEvenByteGm[ALG11_PAIR_COUNT] = {ALG11_GATHER_EVEN_BYTE_TABLE};
__gm__ const int32_t gAlg11GatherOddByteGm[ALG11_PAIR_COUNT] = {ALG11_GATHER_ODD_BYTE_TABLE};
__gm__ const int32_t gAlg11InterleaveReorderByteGm[ALG11_PAIR_COUNT * 2] = {ALG11_INTERLEAVE_REORDER_BYTE_TABLE};
