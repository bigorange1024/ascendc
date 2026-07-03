/**
 * liboqs ML-KEM-1024 KEM 黑盒（Alg.16/17/18/19/20/21）。
 *
 *   keygen <ek_kem.bin> <dk_kem.bin> <hex128_kem_seed>
 *   encaps <ek_kem.bin> <c.bin> <K.bin> <hex64_encaps_seed>
 *   decaps <dk_kem.bin> <c.bin> <K.bin>
 *
 * - keygen：kem_seed = d(32B) || z(32B)，走 OQS_KEM_ml_kem_1024_keypair_derand。
 * - encaps：encaps_seed = m(32B)，走 OQS_KEM_ml_kem_1024_encaps_derand → 输出密文 c 与共享秘密 K。
 * - decaps：无随机，直接 OQS_KEM_ml_kem_1024_decaps(dk, c) → K'；对合法 c 得 K==encaps K，
 *           对被篡改的 c 触发 FIPS 203 隐式拒绝，返回 J(z‖c)=SHAKE256(z‖c,32)。
 */
#include <oqs/oqs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EK_BYTES OQS_KEM_ml_kem_1024_length_public_key
#define DK_BYTES OQS_KEM_ml_kem_1024_length_secret_key
#define CT_BYTES OQS_KEM_ml_kem_1024_length_ciphertext
#define SS_BYTES OQS_KEM_ml_kem_1024_length_shared_secret
#define KEM_SEED_BYTES OQS_KEM_ml_kem_1024_length_keypair_seed
#define ENCAPS_SEED_BYTES OQS_KEM_ml_kem_1024_length_encaps_seed

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_hex64(const char *hex, uint8_t out[64])
{
    if (hex == NULL || strlen(hex) != 128) return -1;
    for (size_t i = 0; i < 64; i++) {
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

static int cmd_encaps(int argc, char **argv)
{
    uint8_t encaps_seed[ENCAPS_SEED_BYTES];
    uint8_t pk[EK_BYTES];
    uint8_t ct[CT_BYTES];
    uint8_t ss[SS_BYTES];

    if (argc != 6) {
        fprintf(stderr, "usage: %s encaps <ek.bin> <c.bin> <K.bin> <hex64_encaps_seed>\n", argv[0]);
        return 1;
    }
    if (parse_hex32(argv[5], encaps_seed) != 0) {
        fprintf(stderr, "invalid hex64 encaps_seed\n");
        return 1;
    }
    FILE *fp = fopen(argv[2], "rb");
    if (fp == NULL) {
        perror(argv[2]);
        return 1;
    }
    if (fread(pk, 1, EK_BYTES, fp) != EK_BYTES) {
        fclose(fp);
        fprintf(stderr, "short read ek\n");
        return 1;
    }
    fclose(fp);
    if (OQS_KEM_ml_kem_1024_encaps_derand(ct, ss, pk, encaps_seed) != OQS_SUCCESS) {
        fprintf(stderr, "OQS_KEM_ml_kem_1024_encaps_derand failed\n");
        return 1;
    }
    if (write_file(argv[3], ct, CT_BYTES) != 0 || write_file(argv[4], ss, SS_BYTES) != 0) {
        return 1;
    }
    return 0;
}

/* decaps：读入 dk 与密文 c，调用 liboqs ML-KEM-1024 Decaps 得共享秘密 K'。
 * decaps 本身不吃随机，故子命令只有三个文件参数、无 hex seed。 */
static int cmd_decaps(int argc, char **argv)
{
    uint8_t sk[DK_BYTES];
    uint8_t ct[CT_BYTES];
    uint8_t ss[SS_BYTES];

    if (argc != 5) {
        fprintf(stderr, "usage: %s decaps <dk.bin> <c.bin> <K.bin>\n", argv[0]);
        return 1;
    }
    /* 读 dk（3168B）。 */
    FILE *fdk = fopen(argv[2], "rb");
    if (fdk == NULL) {
        perror(argv[2]);
        return 1;
    }
    if (fread(sk, 1, DK_BYTES, fdk) != DK_BYTES) {
        fclose(fdk);
        fprintf(stderr, "short read dk\n");
        return 1;
    }
    fclose(fdk);
    /* 读密文 c（1568B）；可能是合法密文或被篡改的密文。 */
    FILE *fct = fopen(argv[3], "rb");
    if (fct == NULL) {
        perror(argv[3]);
        return 1;
    }
    if (fread(ct, 1, CT_BYTES, fct) != CT_BYTES) {
        fclose(fct);
        fprintf(stderr, "short read c\n");
        return 1;
    }
    fclose(fct);
    /* Decaps：合法 c → K==encaps K；篡改 c → 隐式拒绝返回 J(z‖c)。 */
    if (OQS_KEM_ml_kem_1024_decaps(ss, ct, sk) != OQS_SUCCESS) {
        fprintf(stderr, "OQS_KEM_ml_kem_1024_decaps failed\n");
        return 1;
    }
    if (write_file(argv[4], ss, SS_BYTES) != 0) {
        return 1;
    }
    return 0;
}

static int cmd_keygen(int argc, char **argv)
{
    uint8_t kem_seed[KEM_SEED_BYTES];
    uint8_t pk[EK_BYTES];
    uint8_t sk[DK_BYTES];

    if (argc != 5) {
        fprintf(stderr, "usage: %s keygen <ek.bin> <dk.bin> <hex128_kem_seed>\n", argv[0]);
        return 1;
    }
    if (parse_hex64(argv[4], kem_seed) != 0) {
        fprintf(stderr, "invalid hex128 kem_seed\n");
        return 1;
    }
    if (OQS_KEM_ml_kem_1024_keypair_derand(pk, sk, kem_seed) != OQS_SUCCESS) {
        fprintf(stderr, "OQS_KEM_ml_kem_1024_keypair_derand failed\n");
        return 1;
    }
    if (write_file(argv[2], pk, EK_BYTES) != 0 || write_file(argv[3], sk, DK_BYTES) != 0) {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    OQS_init();
    if (argc < 2) {
        fprintf(stderr, "usage: %s keygen|encaps|decaps ...\n", argv[0]);
        OQS_destroy();
        return 1;
    }
    int rc = 1;
    if (strcmp(argv[1], "keygen") == 0) {
        rc = cmd_keygen(argc, argv);
    } else if (strcmp(argv[1], "encaps") == 0) {
        rc = cmd_encaps(argc, argv);
    } else if (strcmp(argv[1], "decaps") == 0) {
        rc = cmd_decaps(argc, argv);
    } else {
        fprintf(stderr, "unknown subcommand: %s\n", argv[1]);
    }
    OQS_destroy();
    return rc;
}
