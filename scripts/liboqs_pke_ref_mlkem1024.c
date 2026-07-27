/**
 * liboqs_pke_ref_mlkem1024.c — ML-KEM-1024（k=4）PKE 黑盒：Alg.13/14/15。
 *
 * 文件名故意带 mlkem1024：本 helper **仅**服务 1024；不对 512/768 做 PKE 交叉。
 * （KEM 交叉见 liboqs_kem_ref + MLKEM_PARAM。）
 *
 * 子命令（与 AscendC 探针 I/O 尺寸一致）：
 *   keygen <ek.bin> <dk.bin> <hex64_d>
 *   encrypt <c.bin> <ek.bin> <m.bin> <coins.bin>
 *   decrypt <m.bin> <dk.bin> <c.bin>
 *
 * KeyGen 走 OQS_KEM_ml_kem_1024_keypair_derand（与 kat_liboqs 一致）；
 * Encrypt/Decrypt 走 mlkem-native C 参考 indcpa_enc/dec（FIPS PKE 层）。
 */
#include <oqs/oqs.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PKE_PK_BYTES OQS_KEM_ml_kem_1024_length_public_key
#define PKE_SK_BYTES (OQS_KEM_ml_kem_1024_length_secret_key - OQS_KEM_ml_kem_1024_length_public_key - 64)
#define PKE_CT_BYTES OQS_KEM_ml_kem_1024_length_ciphertext
#define MSG_BYTES 32
#define COINS_BYTES 32
#define KEM_SK_BYTES OQS_KEM_ml_kem_1024_length_secret_key
#define KEM_SEED_BYTES OQS_KEM_ml_kem_1024_length_keypair_seed

extern void PQCP_MLKEM_NATIVE_MLKEM1024_C_indcpa_enc(uint8_t *c, const uint8_t *m,
                                                     const uint8_t *pk, const uint8_t *coins);
extern void PQCP_MLKEM_NATIVE_MLKEM1024_C_indcpa_dec(uint8_t *m, const uint8_t *c,
                                                     const uint8_t *sk);

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

static int read_file(const char *path, uint8_t *buf, size_t len)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        perror(path);
        return -1;
    }
    if (fread(buf, 1, len, fp) != len) {
        fclose(fp);
        fprintf(stderr, "short read: %s (need %zu)\n", path, len);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int write_file(const char *path, const uint8_t *buf, size_t len)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        perror(path);
        return -1;
    }
    if (fwrite(buf, 1, len, fp) != len) {
        fclose(fp);
        fprintf(stderr, "short write: %s\n", path);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int cmd_keygen(int argc, char **argv)
{
    uint8_t d[32];
    uint8_t kem_seed[KEM_SEED_BYTES];
    uint8_t pk[PKE_PK_BYTES];
    uint8_t sk[KEM_SK_BYTES];

    if (argc != 5) {
        fprintf(stderr, "usage: %s keygen <ek.bin> <dk.bin> <hex64_d>\n", argv[0]);
        return 1;
    }
    if (parse_hex32(argv[4], d) != 0) {
        fprintf(stderr, "invalid hex64 d\n");
        return 1;
    }
    memset(kem_seed, 0, sizeof(kem_seed));
    memcpy(kem_seed, d, 32);
    if (OQS_KEM_ml_kem_1024_keypair_derand(pk, sk, kem_seed) != OQS_SUCCESS) {
        fprintf(stderr, "OQS_KEM_ml_kem_1024_keypair_derand failed\n");
        return 1;
    }
    if (write_file(argv[2], pk, PKE_PK_BYTES) != 0 || write_file(argv[3], sk, PKE_SK_BYTES) != 0) {
        return 1;
    }
    return 0;
}

static int cmd_encrypt(int argc, char **argv)
{
    uint8_t pk[PKE_PK_BYTES];
    uint8_t m[MSG_BYTES];
    uint8_t coins[COINS_BYTES];
    uint8_t c[PKE_CT_BYTES];

    if (argc != 6) {
        fprintf(stderr, "usage: %s encrypt <c.bin> <ek.bin> <m.bin> <coins.bin>\n", argv[0]);
        return 1;
    }
    if (read_file(argv[3], pk, PKE_PK_BYTES) != 0 || read_file(argv[4], m, MSG_BYTES) != 0
        || read_file(argv[5], coins, COINS_BYTES) != 0) {
        return 1;
    }
    PQCP_MLKEM_NATIVE_MLKEM1024_C_indcpa_enc(c, m, pk, coins);
    return write_file(argv[2], c, PKE_CT_BYTES);
}

static int cmd_decrypt(int argc, char **argv)
{
    uint8_t sk[PKE_SK_BYTES];
    uint8_t c[PKE_CT_BYTES];
    uint8_t m[MSG_BYTES];

    if (argc != 5) {
        fprintf(stderr, "usage: %s decrypt <m.bin> <dk.bin> <c.bin>\n", argv[0]);
        return 1;
    }
    if (read_file(argv[3], sk, PKE_SK_BYTES) != 0 || read_file(argv[4], c, PKE_CT_BYTES) != 0) {
        return 1;
    }
    PQCP_MLKEM_NATIVE_MLKEM1024_C_indcpa_dec(m, c, sk);
    return write_file(argv[2], m, MSG_BYTES);
}

int main(int argc, char **argv)
{
    OQS_init();
    if (argc < 2) {
        fprintf(stderr, "usage: %s {keygen|encrypt|decrypt} ...\n", argv[0]);
        OQS_destroy();
        return 1;
    }
    int rc = 1;
    if (strcmp(argv[1], "keygen") == 0) {
        rc = cmd_keygen(argc, argv);
    } else if (strcmp(argv[1], "encrypt") == 0) {
        rc = cmd_encrypt(argc, argv);
    } else if (strcmp(argv[1], "decrypt") == 0) {
        rc = cmd_decrypt(argc, argv);
    } else {
        fprintf(stderr, "unknown subcommand: %s\n", argv[1]);
    }
    OQS_destroy();
    return rc;
}
