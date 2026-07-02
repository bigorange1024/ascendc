/**
 * liboqs ML-KEM-1024 KEM KeyGen 黑盒（Alg.16/19）。
 *
 *   keygen <ek_kem.bin> <dk_kem.bin> <hex128_kem_seed>
 *
 * kem_seed = d(32B) || z(32B)，走 OQS_KEM_ml_kem_1024_keypair_derand。
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
        fprintf(stderr, "usage: %s keygen|encaps ...\n", argv[0]);
        OQS_destroy();
        return 1;
    }
    int rc = 1;
    if (strcmp(argv[1], "keygen") == 0) {
        rc = cmd_keygen(argc, argv);
    } else if (strcmp(argv[1], "encaps") == 0) {
        rc = cmd_encaps(argc, argv);
    } else {
        fprintf(stderr, "unknown subcommand: %s\n", argv[1]);
    }
    OQS_destroy();
    return rc;
}
