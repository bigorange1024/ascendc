# 活跃用例 SIM Total tick 一览

| 项 | 内容 |
|----|------|
| **刷新** | 2026-07-15 |
| **平台** | Ascend910B4 / CAModel（`Total tick`，非 msprof） |
| **口径** | 各目录 **默认配置** 验收 tick；多档 `d` 在备注展开；stub / 未记为 `n/a` |
| **来源优先级** | `STATUS.md` 验收行 → `INDEX.md` → customspec / qa 纪要 |
| **范围** | 活跃 `stable-*` / `exp-*` / `ascendc-tests`；**不含** `frozen/`、`thirdparty/` |
| **维护** | 手工登记（非 CI dump）；见 `qa/TODO.md` **T22** |

---

## examples/stable（交付）

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`stable-fips203-mlkem-pke-keygen-k4`](../examples/stable/stable-fips203-mlkem-pke-keygen-k4/) | **542393** | Alg.13；2 launch；KAT ✓ | STATUS 2026-06-29 |
| [`stable-fips203-mlkem-pke-encrypt-k4`](../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) | **627590** | Alg.14；SIM 主参考 | STATUS |
| [`stable-fips203-mlkem-pke-decrypt-k4`](../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) | **283290** | Alg.15 | STATUS |
| [`stable-fips203-mlkem-kem-keygen-k4`](../examples/stable/stable-fips203-mlkem-kem-keygen-k4/) | **706633** | Alg.19；2 launch；`#交付#` 2026-07-14 | STATUS |

---

## examples/incubating（副本 / 积木）

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`exp-fips203-mlkem-pke-keygen-k4`](../examples/incubating/exp-fips203-mlkem-pke-keygen-k4/) | 542393 | 已晋级 stable；副本同量级 | STATUS→stable |
| [`exp-fips203-mlkem-pke-encrypt-k4`](../examples/incubating/exp-fips203-mlkem-pke-encrypt-k4/) | 627614 | 已晋级；晋级前 SIM | STATUS |
| [`exp-fips203-mlkem-pke-decrypt-k4`](../examples/incubating/exp-fips203-mlkem-pke-decrypt-k4/) | 283290 | 已晋级 | STATUS |
| [`exp-fips203-mlkem-kem-keygen-k4`](../examples/incubating/exp-fips203-mlkem-kem-keygen-k4/) | 707057 | 已晋级；复测 706657 | STATUS |
| [`exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4`](../examples/incubating/exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4/) | ~77958 | 对齐 vec-k4-v2 prefetch | INDEX/customspec |
| [`exp-sepolyvec8-ntt-k8`](../examples/incubating/exp-sepolyvec8-ntt-k8/) | n/a | CPU+SIM PASS；STATUS 未记 tick | INDEX |
| [`exp-fips203-mlkem-pke-stage1-encode-vec`](../examples/incubating/exp-fips203-mlkem-pke-stage1-encode-vec/) | 8520 | 历史 aiv=1 剖面 | legacy-summary |
| [`exp-fips203-mlkem-pke-stage3-routea-mod-vec`](../examples/incubating/exp-fips203-mlkem-pke-stage3-routea-mod-vec/) | 28781 | 历史 aiv=1 剖面 | legacy-summary |
| [`exp-fips203-compress-unified-int-vec-k4`](../examples/incubating/exp-fips203-compress-unified-int-vec-k4/) | 3377 | 向量默认 d=1；d4–11≈3382–3415 | STATUS |
| [`exp-fips203-decompress-unified-int-vec-k4`](../examples/incubating/exp-fips203-decompress-unified-int-vec-k4/) | 3215 | 已验 d=4；d=11 有验、未单记 | STATUS |

---

## ascendc-tests：冒烟 / toy

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`add_custom`](../ascendc-tests/add_custom/) | 9905 | 冒烟；旧 regress 一次 SIM | legacy-summary |
| [`pass-shake128-ops-math-toy`](../ascendc-tests/pass-shake128-ops-math-toy/) | 12285 | case=abc；全 UB | STATUS |
| [`pass-shake256-ascendc-toy`](../ascendc-tests/pass-shake256-ascendc-toy/) | 12285 | case=abc；全 UB | STATUS |
| [`pass-toy-mix-s123-byteencode-k2`](../ascendc-tests/pass-toy-mix-s123-byteencode-k2/) | n/a | INDEX ✓；无 STATUS tick | INDEX |
| [`pass-merged-kyber-mix-ntt256`](../ascendc-tests/pass-merged-kyber-mix-ntt256/) | 10348 | 非 FIPS Tag5T | STATUS 2026-07-13 |

---

## NTT / 行 16–20 积木

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) | **77958** | 全链路 prefetch 默认 | SIM_BENCHMARK |
| [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../ascendc-tests/pass-fix-f203-2s1e-byteencode12-vec-k4/) | 17429 | PREFETCH=1；tile32=25464 | STATUS |
| [`pass-fix-f203-alg11-12-multiplyntts-k4`](../ascendc-tests/pass-fix-f203-alg11-12-multiplyntts-k4/) | ~9031 | MEM_OPS=1 默认 | STATUS |
| [`pass-fix-f203-alg11-12-innerproduct-k4`](../ascendc-tests/pass-fix-f203-alg11-12-innerproduct-k4/) | 43992 | 4×4×1 单 AIV | STATUS |
| [`pass-fix-f203-alg11-12-innerproduct-k4-halfrows`](../ascendc-tests/pass-fix-f203-alg11-12-innerproduct-k4-halfrows/) | 26185 | 双 AIV 半行 | STATUS |
| [`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/) | 30347 | NTT；INTT=30340 | STATUS |

---

## Alg.7 / 8 / KeyGen 子轨

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`pass-fix-f203-alg7-sample-ntt-k4`](../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/) | ~80100 | 全链默认 vec_mins+672B | STATUS |
| [`pass-fix-f203-alg8-cbd-eta2-k4`](../ascendc-tests/pass-fix-f203-alg8-cbd-eta2-k4/) | 18048 | P2 双 AIV 默认 | STATUS |
| [`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4/) | 381544 | 默认双 AIV 672B；单 AIV=733859 | INDEX/STATUS |
| [`pass-fix-f203-alg13-lines8-15-se-k4`](../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/) | 95261 | V3 默认 | STATUS |
| [`pass-fix-f203-alg13-device-keygen-k4`](../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/) | 542339 | 2 launch；STATUS 旧 886801 已过时；Cloud≈542494 | INDEX/customspec |

---

## Alg.14 / 15 PKE device

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`pass-fix-f203-alg14-lines3-15-encrypt-prep-k4`](../ascendc-tests/pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/) | 470502 | 单 launch prep | STATUS |
| [`pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4`](../ascendc-tests/pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/) | ~125000 | SIM 单 launch | qa 2026-07-07 |
| [`pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4`](../ascendc-tests/pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4/) | 56259 | 分组 pack | STATUS |
| [`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../ascendc-tests/pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/) | 154781 | 1 launch compute+尾 | STATUS |
| [`pass-fix-f203-alg14-pke-encrypt-device-k4`](../ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/) | **626139** | 2 launch 全链 Encrypt | STATUS |
| [`pass-fix-f203-alg15-pke-decrypt-device-k4`](../ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4/) | **283252** | 单 kernel | STATUS |

---

## Compress / Decompress / ByteEncode|Decode_d

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`pass-f203-compress-d-vec-k4`](../ascendc-tests/pass-f203-compress-d-vec-k4/) | 3247 | 默认 d=4；d5/10/11=3121/3449/3399 | STATUS |
| [`pass-f203-decompress-d-vec-k4`](../ascendc-tests/pass-f203-decompress-d-vec-k4/) | 3177 | 默认 d=4；d5/10/11=3177/3146/3184 | STATUS |
| [`pass-f203-byteencode-d-vec-k4`](../ascendc-tests/pass-f203-byteencode-d-vec-k4/) | 5435 | 默认 d=4 VEC=1；d5/10/11=5537/6455/6568 | STATUS |
| [`pass-f203-alg6-bytedecode-d-vec-k4`](../ascendc-tests/pass-f203-alg6-bytedecode-d-vec-k4/) | 9186 | 默认 d=4；d5/10/11=5696/6546/6641 | STATUS |
| [`pass-f203-compress-unified-int-vec-k4`](../ascendc-tests/pass-f203-compress-unified-int-vec-k4/) | →exp | canonical 已迁 incubating | INDEX |
| [`pass-f203-decompress-unified-int-vec-k4`](../ascendc-tests/pass-f203-decompress-unified-int-vec-k4/) | →exp | canonical 已迁 incubating | INDEX |

---

## KEM：correctness / device

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`pass-fix-f203-alg19-kem-keygen-device-k4`](../ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/) | **713227** | P1 后 3 次均值；scripts KeyGen 默认 | STATUS |
| [`fix-f203-alg19-kem-keygen-correctness-k4`](../ascendc-tests/fix-f203-alg19-kem-keygen-correctness-k4/) | 742558 | vendor+3 launch oracle | STATUS |
| [`fix-f203-alg20-kem-encaps-correctness-k4`](../ascendc-tests/fix-f203-alg20-kem-encaps-correctness-k4/) | 1029406 | 早前 SIM PASS；STATUS 标待复验 | STATUS |
| [`fix-f203-alg21-kem-decaps-correctness-k4`](../ascendc-tests/fix-f203-alg21-kem-decaps-correctness-k4/) | n/a | SIM 2-session PASS；未记 Total tick | STATUS |
| [`fix-f203-alg20-kem-encaps-device-k4`](../ascendc-tests/fix-f203-alg20-kem-encaps-device-k4/) | **721010** | T19a PASS（prep H/G + stable Encrypt；`SIM_DIRECT=1`） | STATUS 2026-07-15 |
| [`fix-f203-alg21-kem-decaps-device-k4`](../ascendc-tests/fix-f203-alg21-kem-decaps-device-k4/) | n/a | T19b/c 待开工 | STATUS |
