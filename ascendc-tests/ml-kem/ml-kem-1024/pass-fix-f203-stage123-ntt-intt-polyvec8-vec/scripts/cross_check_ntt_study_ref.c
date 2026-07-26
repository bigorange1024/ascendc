/*
 * @file cross_check_ntt_study_ref.c
 * @brief ntt_study C 参考对拍小工具（由 cross_check_ntt_study_c.py 编译调用）。
 *
 * 流水线位置：独立于 run.sh；读 src.bin，调用 Tag5T / F203 Tag3 批 NTT|INTT，
 * 与 ref.bin（Python golden 或设备 dst 或交付 golden）逐系数比较。
 *
 * 用法:
 *   cross_check_ntt_study_ref <src.bin> <ref.bin> <ntt|intt> [--tag5|--tag3|--both]
 *
 * 与 golden 关系：本程序是外部 oracle 对照，不生成 golden；exit 0 表示所选路径 maxdiff=0。
 *
 * 语义：K=8、N=256；LUT 取自 transpose_mlkem_luts_i8.h（与设备 host 写入的 bin 同源）。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "tag4_f203.h"
#include "tag5_natural_batch_transpose.h"
#include "transpose_mlkem_luts_i8.h"

#define K 8
#define N 256

/**
 * 从 path 读取恰好 n 个 int32 到 buf。
 * @return 0 成功；1 打开失败或长度不符
 */
static int read_i32(const char *path, int32_t *buf, size_t n)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        perror(path);
        return 1;
    }
    size_t got = fread(buf, sizeof(int32_t), n, f);
    fclose(f);
    if (got != n)
    {
        fprintf(stderr, "%s: size mismatch (got %zu expect %zu int32)\n", path, got, n);
        return 1;
    }
    return 0;
}

/**
 * 逐元素 |a[i]-b[i]|，打印 label 下的 max 与非零个数。
 * @return maxdiff（0 表示完全一致）
 */
static int cmp_label(const char *label, const int32_t *a, const int32_t *b, size_t n)
{
    int max = 0;
    int nz = 0;
    int first = -1;
    for (size_t i = 0; i < n; ++i)
    {
        int d = abs((int)a[i] - (int)b[i]);
        if (d != 0)
        {
            if (first < 0)
            {
                first = (int)i;
            }
            nz++;
            if (d > max)
            {
                max = d;
            }
        }
    }
    printf("[%s] max=%d nz=%d\n", label, max, nz);
    return max;
}

/**
 * Tag5T 自然批：按 is_ntt 选择 NTT/INTT limb6 LUT，写出 out[K*N]。
 * 与本探针设备三段式同族（平面/紧凑实现细节在库内）。
 */
static void run_tag5(int32_t *out, const int32_t *src, int is_ntt)
{
    const int8_t *lut = is_ntt ? kMlkemLimb6Ntt_T_i8 : kMlkemLimb6Intt_T_i8;
    Tag5T_MlkemNaturalBatchNtt(out, lut, src, N, 3329, K);
}

/**
 * F203 Tag3 批 NTT/INTT（对照路径，非本探针设备布局）。
 */
static void run_tag3(int32_t *out, const int32_t *src, int is_ntt)
{
    F203MlkemTag3PolyvecBatchNttIntt(out, src, K, is_ntt ? 1 : 0);
}

/**
 * 主入口：解析 argv，读 src/ref，按 --tag5/--tag3/--both 比较。
 * @return 0 全部 maxdiff=0；1 有差异；2 用法错误
 */
int main(int argc, char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr,
                "usage: %s <src.bin> <ref.bin> <ntt|intt> [--tag5|--tag3|--both]\n",
                argv[0] != NULL ? argv[0] : "cross_check_ntt_study_ref");
        return 2;
    }

    /* 第 5 参默认 --both：同时跑 Tag5T 与 Tag3，并互比 */
    const char *which = "both";
    if (argc >= 5)
    {
        which = argv[4];
    }

    int is_ntt = 1;
    if (strcmp(argv[3], "intt") == 0)
    {
        is_ntt = 0;
    }
    else if (strcmp(argv[3], "ntt") != 0)
    {
        fprintf(stderr, "mode must be ntt or intt\n");
        return 2;
    }

    int32_t src[K * N];
    int32_t ref[K * N];
    int32_t tag5[K * N];
    int32_t tag3[K * N];

    if (read_i32(argv[1], src, K * N) != 0)
    {
        return 1;
    }
    if (read_i32(argv[2], ref, K * N) != 0)
    {
        return 1;
    }

    int fail = 0;
    /* which 可能是字面 "--both" 或历史兼容的 "both" */
    int do_tag5 = (strcmp(which, "--tag5") == 0 || strcmp(which, "--both") == 0);
    int do_tag3 = (strcmp(which, "--tag3") == 0 || strcmp(which, "--both") == 0);

    if (do_tag5)
    {
        run_tag5(tag5, src, is_ntt);
        fail |= cmp_label("ref_vs_tag5t_c", ref, tag5, K * N) != 0;
    }
    if (do_tag3)
    {
        run_tag3(tag3, src, is_ntt);
        fail |= cmp_label("ref_vs_f203_tag3_c", ref, tag3, K * N) != 0;
    }
    if (do_tag5 && do_tag3)
    {
        /* 两路 C 参考互比，确认 Tag5T 与 Tag3 在本输入上一致 */
        fail |= cmp_label("tag5t_c_vs_f203_tag3_c", tag5, tag3, K * N) != 0;
    }

    return fail != 0 ? 1 : 0;
}
