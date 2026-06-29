/**
 * liboqs ML-KEM-1024（k=4）PKE KeyGen 黑盒参考：Alg.13 indcpa_keypair_derand。
 *
 * 本仓 PKE 输出尺寸与 FIPS ML-KEM-1024 参数集一致（非 OQS 命名里的 ml_kem_768）：
 *   ek_PKE = INDCPA pk = polyvec(t)‖ρ，1568 B
 *   dk_PKE = INDCPA sk = polyvec(s)，1536 B
 *
 * 输入：32 字节 coins = FIPS 的 d（由 SEED_D 经 derand 得到）。
 * 调用 OQS_KEM_ml_kem_1024_keypair_derand；其 coins[0:32] 即传入 indcpa。
 */
#include <oqs/oqs.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PKE_PK_BYTES OQS_KEM_ml_kem_1024_length_public_key
#define PKE_SK_BYTES (OQS_KEM_ml_kem_1024_length_secret_key - OQS_KEM_ml_kem_1024_length_public_key - 64)
#define KEM_SK_BYTES OQS_KEM_ml_kem_1024_length_secret_key
#define KEM_SEED_BYTES OQS_KEM_ml_kem_1024_length_keypair_seed

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_hex32(const char *hex, uint8_t out[32])
{
    if (hex == NULL || strlen(hex) != 64) return -1;
    for (size_t i = 0; i < 32; i++) {
        int hi = hex_nibble(hex[2 * i]);
        int lo = hex_nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 0;
}

static int write_file(const char *path, const uint8_t *buf, size_t len)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) { perror(path); return -1; }
    if (fwrite(buf, 1, len, fp) != len) {
        fclose(fp);
        fprintf(stderr, "short write: %s\n", path);
        return -1;
    }
    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    uint8_t d[32];
    uint8_t kem_seed[KEM_SEED_BYTES];
    uint8_t pk[PKE_PK_BYTES];
    uint8_t sk[KEM_SK_BYTES];
    const char *ek_path;
    const char *dk_path;

    OQS_init(); if (0) {
        return 1;
    }

    if (argc == 4) {
        ek_path = argv[1];
        dk_path = argv[2];
        if (parse_hex32(argv[3], d) != 0) {
            fprintf(stderr, "usage: %s <ek.bin> <dk.bin> <hex64_d>\n", argv[0]);
            OQS_destroy();
            return 1;
        }
    } else if (argc == 3) {
        ek_path = argv[1];
        dk_path = argv[2];
        if (fread(d, 1, sizeof(d), stdin) != sizeof(d)) {
            fprintf(stderr, "expected 32 bytes d on stdin\n");
            OQS_destroy();
            return 1;
        }
    } else {
        fprintf(stderr, "usage: %s <ek.bin> <dk.bin> <hex64_d>\n", argv[0]);
        OQS_destroy();
        return 1;
    }

    memset(kem_seed, 0, sizeof(kem_seed));
    memcpy(kem_seed, d, 32);

    if (OQS_KEM_ml_kem_1024_keypair_derand(pk, sk, kem_seed) != OQS_SUCCESS) {
        fprintf(stderr, "OQS_KEM_ml_kem_1024_keypair_derand failed\n");
        OQS_destroy();
        return 1;
    }

    if (write_file(ek_path, pk, PKE_PK_BYTES) != 0 ||
        write_file(dk_path, sk, PKE_SK_BYTES) != 0) {
        OQS_destroy();
        return 1;
    }

    OQS_destroy();
    return 0;
}
