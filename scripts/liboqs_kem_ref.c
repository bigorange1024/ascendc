/**
 * liboqs ML-KEM KEM 黑盒（Alg.16/17/18/19/20/21），支持参数组 512 / 768 / 1024。
 *
 * 参数选择（优先级从高到低）：
 *   1) 全局选项：liboqs_kem_ref --param 512|768|1024 <subcommand> ...
 *   2) 环境变量：MLKEM_PARAM=512|768|1024
 *   3) 默认：1024（兼容旧调用）
 *
 * 子命令：
 *   keygen <ek_kem.bin> <dk_kem.bin> <hex128_kem_seed>
 *   encaps <ek_kem.bin> <c.bin> <K.bin> <hex64_encaps_seed>
 *   decaps <dk_kem.bin> <c.bin> <K.bin>
 *
 * 实现走 OQS_KEM 泛型 API（keypair_derand / encaps_derand / decaps），
 * 长度与算法名由 liboqs 对象提供，禁止再写死 1024 常量。
 */
#include <oqs/oqs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    OQS_KEM *kem;
    const char *alg_name;
    const char *param_tag; /* "512" / "768" / "1024" */
} KemCtx;

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_hex_n(const char *hex, uint8_t *out, size_t n)
{
    if (hex == NULL || strlen(hex) != 2 * n) return -1;
    for (size_t i = 0; i < n; i++) {
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

static const char *resolve_alg_name(const char *param)
{
    if (param == NULL) return NULL;
    if (strcmp(param, "512") == 0 || strcmp(param, "ml-kem-512") == 0
        || strcmp(param, "ML-KEM-512") == 0) {
        return OQS_KEM_alg_ml_kem_512;
    }
    if (strcmp(param, "768") == 0 || strcmp(param, "ml-kem-768") == 0
        || strcmp(param, "ML-KEM-768") == 0) {
        return OQS_KEM_alg_ml_kem_768;
    }
    if (strcmp(param, "1024") == 0 || strcmp(param, "ml-kem-1024") == 0
        || strcmp(param, "ML-KEM-1024") == 0) {
        return OQS_KEM_alg_ml_kem_1024;
    }
    return NULL;
}

static const char *param_tag_from_alg(const char *alg)
{
    if (strcmp(alg, OQS_KEM_alg_ml_kem_512) == 0) return "512";
    if (strcmp(alg, OQS_KEM_alg_ml_kem_768) == 0) return "768";
    if (strcmp(alg, OQS_KEM_alg_ml_kem_1024) == 0) return "1024";
    return "?";
}

static int kem_open(KemCtx *ctx, const char *param)
{
    const char *alg = resolve_alg_name(param);
    if (alg == NULL) {
        fprintf(stderr,
                "[liboqs_kem_ref] invalid MLKEM_PARAM/param=%s (want 512|768|1024)\n",
                param ? param : "(null)");
        return -1;
    }
    ctx->kem = OQS_KEM_new(alg);
    if (ctx->kem == NULL) {
        fprintf(stderr, "[liboqs_kem_ref] OQS_KEM_new(%s) failed (algo disabled?)\n", alg);
        return -1;
    }
    ctx->alg_name = alg;
    ctx->param_tag = param_tag_from_alg(alg);
    return 0;
}

static void kem_close(KemCtx *ctx)
{
    if (ctx->kem != NULL) {
        OQS_KEM_free(ctx->kem);
        ctx->kem = NULL;
    }
}

static int cmd_keygen(KemCtx *ctx, int argc, char **argv)
{
    OQS_KEM *kem = ctx->kem;
    const size_t seed_len = kem->length_keypair_seed;
    const size_t pk_len = kem->length_public_key;
    const size_t sk_len = kem->length_secret_key;
    uint8_t *seed = NULL;
    uint8_t *pk = NULL;
    uint8_t *sk = NULL;
    int rc = 1;

    if (argc != 5) {
        fprintf(stderr, "usage: %s [--param N] keygen <ek.bin> <dk.bin> <hex128_kem_seed>\n",
                argv[0]);
        return 1;
    }
    if (seed_len != 64) {
        fprintf(stderr, "[liboqs_kem_ref] unexpected keypair_seed len=%zu\n", seed_len);
        return 1;
    }

    seed = malloc(seed_len);
    pk = malloc(pk_len);
    sk = malloc(sk_len);
    if (seed == NULL || pk == NULL || sk == NULL) {
        fprintf(stderr, "oom\n");
        goto done;
    }
    if (parse_hex_n(argv[4], seed, seed_len) != 0) {
        fprintf(stderr, "invalid hex kem_seed (want %zu bytes)\n", seed_len);
        goto done;
    }
    if (OQS_KEM_keypair_derand(kem, pk, sk, seed) != OQS_SUCCESS) {
        fprintf(stderr, "OQS_KEM_keypair_derand(%s) failed\n", ctx->alg_name);
        goto done;
    }
    if (write_file(argv[2], pk, pk_len) != 0 || write_file(argv[3], sk, sk_len) != 0) {
        goto done;
    }
    rc = 0;
done:
    free(seed);
    free(pk);
    free(sk);
    return rc;
}

static int cmd_encaps(KemCtx *ctx, int argc, char **argv)
{
    OQS_KEM *kem = ctx->kem;
    const size_t encaps_seed_len = kem->length_encaps_seed;
    const size_t pk_len = kem->length_public_key;
    const size_t ct_len = kem->length_ciphertext;
    const size_t ss_len = kem->length_shared_secret;
    uint8_t *encaps_seed = NULL;
    uint8_t *pk = NULL;
    uint8_t *ct = NULL;
    uint8_t *ss = NULL;
    int rc = 1;

    if (argc != 6) {
        fprintf(stderr,
                "usage: %s [--param N] encaps <ek.bin> <c.bin> <K.bin> <hex64_encaps_seed>\n",
                argv[0]);
        return 1;
    }
    if (encaps_seed_len != 32) {
        fprintf(stderr, "[liboqs_kem_ref] unexpected encaps_seed len=%zu\n", encaps_seed_len);
        return 1;
    }

    encaps_seed = malloc(encaps_seed_len);
    pk = malloc(pk_len);
    ct = malloc(ct_len);
    ss = malloc(ss_len);
    if (encaps_seed == NULL || pk == NULL || ct == NULL || ss == NULL) {
        fprintf(stderr, "oom\n");
        goto done;
    }
    if (parse_hex_n(argv[5], encaps_seed, encaps_seed_len) != 0) {
        fprintf(stderr, "invalid hex encaps_seed\n");
        goto done;
    }
    if (read_file(argv[2], pk, pk_len) != 0) {
        goto done;
    }
    if (OQS_KEM_encaps_derand(kem, ct, ss, pk, encaps_seed) != OQS_SUCCESS) {
        fprintf(stderr, "OQS_KEM_encaps_derand(%s) failed\n", ctx->alg_name);
        goto done;
    }
    if (write_file(argv[3], ct, ct_len) != 0 || write_file(argv[4], ss, ss_len) != 0) {
        goto done;
    }
    rc = 0;
done:
    free(encaps_seed);
    free(pk);
    free(ct);
    free(ss);
    return rc;
}

static int cmd_decaps(KemCtx *ctx, int argc, char **argv)
{
    OQS_KEM *kem = ctx->kem;
    const size_t sk_len = kem->length_secret_key;
    const size_t ct_len = kem->length_ciphertext;
    const size_t ss_len = kem->length_shared_secret;
    uint8_t *sk = NULL;
    uint8_t *ct = NULL;
    uint8_t *ss = NULL;
    int rc = 1;

    if (argc != 5) {
        fprintf(stderr, "usage: %s [--param N] decaps <dk.bin> <c.bin> <K.bin>\n", argv[0]);
        return 1;
    }

    sk = malloc(sk_len);
    ct = malloc(ct_len);
    ss = malloc(ss_len);
    if (sk == NULL || ct == NULL || ss == NULL) {
        fprintf(stderr, "oom\n");
        goto done;
    }
    if (read_file(argv[2], sk, sk_len) != 0 || read_file(argv[3], ct, ct_len) != 0) {
        goto done;
    }
    if (OQS_KEM_decaps(kem, ss, ct, sk) != OQS_SUCCESS) {
        fprintf(stderr, "OQS_KEM_decaps(%s) failed\n", ctx->alg_name);
        goto done;
    }
    if (write_file(argv[4], ss, ss_len) != 0) {
        goto done;
    }
    rc = 0;
done:
    free(sk);
    free(ct);
    free(ss);
    return rc;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [--param 512|768|1024] keygen|encaps|decaps ...\n"
            "  param also via env MLKEM_PARAM (default 1024)\n",
            argv0);
}

int main(int argc, char **argv)
{
    const char *param = getenv("MLKEM_PARAM");
    int argi = 1;
    KemCtx ctx = {0};
    int rc = 1;

    OQS_init();

    if (argi < argc && strcmp(argv[argi], "--param") == 0) {
        if (argi + 1 >= argc) {
            usage(argv[0]);
            OQS_destroy();
            return 1;
        }
        param = argv[argi + 1];
        argi += 2;
    }
    if (param == NULL || param[0] == '\0') {
        param = "1024";
    }

    if (argi >= argc) {
        usage(argv[0]);
        OQS_destroy();
        return 1;
    }

    /* 把 argv[0] 与子命令对齐到 cmd_* 期望的 argc/argv 布局：
     * cmd 仍看 argv[0]=prog, argv[1]=sub, argv[2]=...
     * 因此构造局部指针数组。 */
    {
        int n = argc - argi + 1;
        char **av = calloc((size_t)n, sizeof(char *));
        if (av == NULL) {
            fprintf(stderr, "oom\n");
            OQS_destroy();
            return 1;
        }
        av[0] = argv[0];
        for (int i = 0; i < argc - argi; i++) {
            av[1 + i] = argv[argi + i];
        }

        if (kem_open(&ctx, param) != 0) {
            free(av);
            OQS_destroy();
            return 1;
        }
        fprintf(stderr, "[liboqs_kem_ref] param=%s alg=%s ek=%zu dk=%zu c=%zu\n",
                ctx.param_tag, ctx.alg_name, ctx.kem->length_public_key,
                ctx.kem->length_secret_key, ctx.kem->length_ciphertext);

        if (strcmp(av[1], "keygen") == 0) {
            rc = cmd_keygen(&ctx, n, av);
        } else if (strcmp(av[1], "encaps") == 0) {
            rc = cmd_encaps(&ctx, n, av);
        } else if (strcmp(av[1], "decaps") == 0) {
            rc = cmd_decaps(&ctx, n, av);
        } else {
            fprintf(stderr, "unknown subcommand: %s\n", av[1]);
            usage(argv[0]);
        }

        kem_close(&ctx);
        free(av);
    }

    OQS_destroy();
    return rc;
}
