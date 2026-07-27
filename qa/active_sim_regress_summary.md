# 活跃用例 SIM Total tick 一览

| 项 | 内容 |
|----|------|
| **刷新** | 2026-07-27（新增 **ML-KEM-512 W2 D13 PKE KeyGen + D15 PKE Decrypt** Cloud SIM tick；W0/W1 已登记；ML-KEM-768 全表沿用 07-26；Decaps CT k4 行沿用 07-24） |
| **平台** | Ascend910B4 / CAModel（`Total tick`，非 msprof） |
| **口径** | 各目录 **默认配置** 验收 tick；多档 `d` 在备注展开；stub / 未记为 `n/a`；Decaps 合法路径默认 `decaps_2session`（CT）或交付 STATUS 口径 |
| **来源优先级** | `STATUS.md` 验收行 → `INDEX.md` → customspec / qa 纪要 |
| **范围** | 活跃 `stable-*` / `exp-*` / `ascendc-tests`；**不含** `frozen/`、`thirdparty/` |
| **维护** | 手工登记（非 CI dump）；见 `qa/TODO.md` **T22** |

---

## ML-KEM-768（k=3）积木 — W0 / W1

> Cloud / `SIM_DIRECT=1` / Ascend910B4（2026-07-26）。**非** msprof；首版正确性优先，**非**生产性能基线。
> 路径根：[`ascendc-tests/ml-kem/ml-kem-768/`](../ascendc-tests/ml-kem/ml-kem-768/INDEX.md)。对照 k4 同行见下文各积木节。

| ID | 目录 | SIM tick（默认） | 备注 | 来源 |
|----|------|------------------|------|------|
| **B1** | [`pass-fix-f203-compress-decompress-du10-dv4-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-compress-decompress-du10-dv4-k3/) | compress d4/**3196** d10/**3420**；decompress d4/**3304** d10/**3247** | 编排 d∈{4,10} | STATUS 2026-07-26 |
| **B2** | [`pass-fix-f203-byteencode-decode-d-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-byteencode-decode-d-k3/) | enc4/**5423** dec4/**9351**；enc10/**6539** dec10/**6572**；encode12/**17511** | encode12 仍用 k4 几何作 d=12 算法探针 | STATUS 2026-07-26 |
| **B3** | [`pass-fix-f203-alg8-cbd-eta2-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg8-cbd-eta2-k3/) | **14949** | polyvec6；`blockDim=2` | STATUS 2026-07-26 |
| **B4** | [`pass-fix-f203-alg7-sample-ntt-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg7-sample-ntt-k3/) | **80783** | 默认 (j,i)=(0,0)；G 内 k=3 | STATUS 2026-07-26 |
| **B5** | [`pass-fix-f203-stage123-ntt-intt-polyvec6-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-stage123-ntt-intt-polyvec6-k3/) | NTT **26651** / INTT **26672** | `[6,256]`；AIV 连续 3+3 | STATUS 2026-07-26 |
| **B6** | [`pass-fix-f203-alg11-12-multiply-inner-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg11-12-multiply-inner-k3/) | multiply **9416** / inner **21881** | Inner `P=3`；AIV **2+1** | STATUS 2026-07-26 |
| **D13** | [`pass-fix-f203-alg13-device-keygen-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg13-device-keygen-k3/) | **373426** | 2 launch；prep 5+4；compute polyvec6 + inner 2+1；ek/dk golden PASS | STATUS 2026-07-26 |
| **D14** | [`pass-fix-f203-alg14-pke-encrypt-device-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg14-pke-encrypt-device-k3/) | **507605** | 2 launch；prep Â[9]+re[7]；compute INTT batch4 + d10/d4 pack；c golden PASS | STATUS 2026-07-26 |
| **D15** | [`pass-fix-f203-alg15-pke-decrypt-device-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg15-pke-decrypt-device-k3/) | **222032** | 1 fused launch；dk=1152+c=1088；d10/d4 unpack；m golden PASS | STATUS 2026-07-26 |
| **D19** | [`pass-fix-f203-alg19-kem-keygen-device-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg19-kem-keygen-device-k3/) | **510775** | 2 launch；D13 prep+compute；Alg.16 tail embedded；ek_kem/dk_kem golden PASS | STATUS 2026-07-26 |
| **D20** | [`pass-fix-f203-alg20-kem-encaps-device-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg20-kem-encaps-device-k3/) | **592129** | 2 launch；KEM 头 + D14 k3 Encrypt；c/K golden PASS | STATUS 2026-07-26 |
| **D21** | [`pass-fix-f203-alg21-kem-decaps-device-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg21-kem-decaps-device-k3/) | **818285**（D**220767** + E**597518**） | SIM 3 launch；D15 k3 Decrypt + D14-shaped re-encrypt + FO；K golden PASS | STATUS 2026-07-26 |
| **D21ct** | [`pass-fix-f203-alg21-kem-decaps-device-ct-k3`](../ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg21-kem-decaps-device-ct-k3/) | accept **826458**（D**220868** + E**605590**）；reject **823002**（D**220680** + E**602322**） | CT 默认 `decaps_2session`；accept K max=0；reject `K=J(z‖c)` 且 `reject≠accept` | STATUS 2026-07-26 |

### 与 k=4 同名积木对照（量级参考，非验收门禁）

| 能力 | 768 (k3) tick | 1024 (k4) 登记 tick | 备注 |
|------|---------------|---------------------|------|
| CBD η=2 | **14949**（6 行） | 18048（8 行，P2） | 行数↓，tick 同量级 |
| SampleNTT 单 poly | **80783** | ~80100 | 几乎持平（XOF/rej 主导） |
| Stage123 NTT | **26651**（polyvec6） | 30347（polyvec8） | 批更小 |
| MultiplyNTTs | **9416** | ~9031 | 同量级 |
| InnerProduct | **21881**（2+1） | 26185（halfrows 2+2） | 输出行 3 vs 4 |

W2 device：D13/D14/D15 已登记；ML-KEM-768 PKE 三段 device 均 CPU+SIM 绿。W3 device：D19 KEM KeyGen、D20 Encaps、D21 Decaps 与 D21ct CT 已登记。

---

## ML-KEM-512（k=2）积木 — W0 / W1 / W2

> Cloud / `SIM_DIRECT=1` / Ascend910B4（2026-07-27）。512 W0/W1 积木探针与 W2 device；正确性优先，非生产性能基线。
> 路径根：[`ascendc-tests/ml-kem/ml-kem-512/`](../ascendc-tests/ml-kem/ml-kem-512/INDEX.md)。

| ID | 目录 | SIM tick（默认） | 备注 | 来源 |
|----|------|------------------|------|------|
| **B2** | [`pass-fix-f203-byteencode-decode-d-k2`](../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-byteencode-decode-d-k2/) | enc4/**5407** dec4/**9340**；enc10/**6629** dec10/**6561**；encode12/**17613** | d=4/10 编解码；encode12 仍用 k4 几何作 d=12 算法探针 | STATUS 2026-07-27 |
| **B3a** | [`pass-fix-f203-alg8-cbd-eta2-k2`](../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg8-cbd-eta2-k2/) | **11377** | polyvec4；η=2；`blockDim=2`；AIV0 `{0,2}` / AIV1 `{1,3}` | STATUS 2026-07-27 |
| **B3b** | [`pass-fix-f203-alg8-cbd-eta3-k2`](../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg8-cbd-eta3-k2/) | **13566** | polyvec4；η=3；`blockDim=2`；AIV0 `{0,2}` / AIV1 `{1,3}` | STATUS 2026-07-27 |
| **B4** | [`pass-fix-f203-alg7-sample-ntt-k2`](../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg7-sample-ntt-k2/) | **80235** | SampleNTT 单 poly；`G(d||2)`；2×2 matrix CPU PASS | STATUS 2026-07-27 |
| **B5** | [`pass-fix-f203-stage123-ntt-intt-polyvec4-k2`](../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-stage123-ntt-intt-polyvec4-k2/) | NTT **22921** / INTT **22836** | true polyvec4；MIX `blockDim=1`；AIV 连续 `{0,1}`/`{2,3}`；Cube HW pad m→16 非假 poly | STATUS 2026-07-27 |
| **B6** | [`pass-fix-f203-alg11-12-multiply-inner-k2`](../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg11-12-multiply-inner-k2/) | multiply **9290** / inner **12603** | Inner `P_OUT=S_VEC=2`；AIV **1+1**；`t_hat[2,256]` | STATUS 2026-07-27 |
| **D13** | [`pass-fix-f203-alg13-device-keygen-k2`](../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg13-device-keygen-k2/) | **230102** | 2 launch；prep Â[4] 2+2 + polyvec4 CBD-η3；compute `S0[8,256]` + inner 1+1；ek/dk 800/768 golden PASS | STATUS 2026-07-27 |
| **D15** | [`pass-fix-f203-alg15-pke-decrypt-device-k2`](../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg15-pke-decrypt-device-k2/) | **168975** | 1 fused launch；dk=768+c=768；c1=2×320、c2=128；m golden PASS | STATUS 2026-07-27 |

---

## examples/stable（交付）

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`stable-fips203-mlkem-pke-keygen-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-keygen-k4/) | **542393** | Alg.13；2 launch；KAT ✓ | STATUS 2026-06-29 |
| [`stable-fips203-mlkem-pke-encrypt-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/) | **627590** | Alg.14；SIM 主参考 | STATUS |
| [`stable-fips203-mlkem-pke-decrypt-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4/) | **283290** | Alg.15 | STATUS |
| [`stable-fips203-mlkem-kem-keygen-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4/) | **706633** | Alg.19；2 launch；`#交付#`；roundtrip 复测 Cloud **691727** / WSL **700879** | STATUS + rt 2026-07-20 |
| [`stable-fips203-mlkem-kem-encaps-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/) | **721119** | Alg.20；SIM 2；`#验收#`；roundtrip 复测 Cloud **720048** / WSL **719417** | STATUS + rt 2026-07-20 |
| [`stable-fips203-mlkem-kem-decaps-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4/) | **1041906**（D**286865**+E**755041**） | Alg.21 **交付**（无 `-ct`）；**T19i** SIM **3**；登记取 Cloud 端到端全绿 `20260720_102426_216972`；T19i 单算子验收曾 **1050620**；WSL 单独 Decaps **1041521**（D**286698**+E**754823**）；[`stable_kem_liboqs_roundtrip.sh`](../scripts/stable_kem_liboqs_roundtrip.sh) | STATUS + rt 2026-07-20 |
| [`stable-fips203-mlkem-kem-decaps-ct-k4`](../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-ct-k4/) | **1050646**（D**286866**+E**763780**） | Alg.21 **CT 专题**；Cloud / `SIM_DIRECT=1` / `decaps_2session`；合法 `K` max=0；拒绝 SIM≈D**286679**+E**763735**；KAT CPU×10+SIM×3 / roundtrip CPU+SIM 全绿；**非** `scripts/` 默认 | STATUS 2026-07-24 |

> **KEM↔liboqs roundtrip（2026-07-20）**：Cloud 端到端 CPU+SIM 全绿（fixture `output/stable_kem_liboqs_rt/20260720_102426_216972/`）。WSL 同脚本：CPU 全绿；连续 SIM 偶发 Decaps `tcache`（见当日纪要）；同 fixture 单独 Decaps SIM 仍绿（fixture `…/20260720_185052_42894/`）。

> **2026-07-24 Decaps CT**：`-ct` 三树合法 SIM 合计均约 **1.05M**（D≈**287k**+E≈**764k**）；拒绝路径同量级。交付无 `-ct` 行**不**用 CT 数覆盖。详见 [`qa/2026-07/2026-07-24-第7章CT与Decaps-device-PASS.md`](2026-07/2026-07-24-第7章CT与Decaps-device-PASS.md)。

> **2026-07-20**：`probe-f203-alg{19,20,21}-*-correctness-k4` **已冻结**（见 [`frozen/INDEX.md`](../ascendc-tests/frozen/INDEX.md)）；**勿**再跑 / 勿作回归默认。历史 tick 仅见各 `FROZEN.md`。

---

## examples/incubating（副本 / 积木）

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`exp-fips203-mlkem-pke-keygen-k3`](../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-keygen-k3/) | **373429** | ML-KEM-768 E13；2 launch；prep 5+4 + compute polyvec6；ek/dk golden PASS | STATUS 2026-07-26 |
| [`exp-fips203-mlkem-pke-encrypt-k3`](../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-encrypt-k3/) | **507633** | ML-KEM-768 E14；2 launch；Â[9]+re[7]；INTT batch4；c golden PASS | STATUS 2026-07-26 |
| [`exp-fips203-mlkem-pke-decrypt-k3`](../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-decrypt-k3/) | **222073** | ML-KEM-768 E15；1 fused launch；dk/c→m；m golden PASS | STATUS 2026-07-26 |
| [`exp-fips203-mlkem-kem-keygen-k3`](../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-keygen-k3/) | **510867** | ML-KEM-768 E19；2 launch；E13/D19 geometry + Alg.16 tail；ek_kem/dk_kem golden PASS | STATUS 2026-07-26 |
| [`exp-fips203-mlkem-kem-encaps-k3`](../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-encaps-k3/) | **590261** | ML-KEM-768 E20；2 launch；KEM head + E14/D20 geometry；c/K golden PASS | STATUS 2026-07-26 |
| [`exp-fips203-mlkem-kem-decaps-k3`](../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-decaps-k3/) | accept **820230**（D**221059** + E**599171**）；reject **822500**（D**220692** + E**601808**） | ML-KEM-768 E21；delivery 默认 `decaps_1session`；K golden PASS；reject `K=J(z‖c)` | STATUS 2026-07-26 |
| [`exp-fips203-mlkem-kem-decaps-ct-k3`](../examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-decaps-ct-k3/) | accept **826115**（D**220727** + E**605388**）；reject **825836**（D**220787** + E**605049**） | ML-KEM-768 E21ct；CT 默认 `decaps_2session`；accept K max=0；reject `K=J(z‖c)` 且 `reject≠accept` | STATUS 2026-07-26 |
| [`exp-fips203-mlkem-pke-keygen-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-keygen-k4/) | 542393 | 已晋级 stable；副本同量级 | STATUS→stable |
| [`exp-fips203-mlkem-pke-encrypt-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-encrypt-k4/) | 627614 | 已晋级；晋级前 SIM | STATUS |
| [`exp-fips203-mlkem-pke-decrypt-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-decrypt-k4/) | 283290 | 已晋级 | STATUS |
| [`exp-fips203-mlkem-kem-keygen-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-keygen-k4/) | 707057 | 已晋级；复测 706657 | STATUS |
| [`exp-fips203-mlkem-kem-encaps-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-encaps-k4/) | **721211** | 已晋级 stable；副本 tick（非零 `m` **721033**） | STATUS 2026-07-15 |
| [`exp-fips203-mlkem-kem-decaps-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-decaps-k4/) | **1050781**（D**286846**+E**763935**） | Alg.21 **交付**副本（无 `-ct`）；**T19i** SIM **3**；已晋级 stable | STATUS 2026-07-20 |
| [`exp-fips203-mlkem-kem-decaps-ct-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-decaps-ct-k4/) | **1050487**（D**286829**+E**763658**） | Alg.21 **CT 专题**；合法 SIM；拒绝≈D**286666**+E**763697**；已复制晋级 `stable-…-decaps-ct-k4` | STATUS 2026-07-24 |
| [`exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4/) | ~77958 | 对齐 vec-k4-v2 prefetch | INDEX/customspec |
| [`exp-sepolyvec8-ntt-k8`](../examples/incubating/ml-kem/ml-kem-1024/exp-sepolyvec8-ntt-k8/) | n/a | CPU+SIM PASS；STATUS 未记 tick | INDEX |
| [`exp-fips203-mlkem-pke-stage1-encode-vec`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-stage1-encode-vec/) | 8520 | 历史 aiv=1 剖面 | legacy-summary |
| [`exp-fips203-mlkem-pke-stage3-routea-mod-vec`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-stage3-routea-mod-vec/) | 28781 | 历史 aiv=1 剖面 | legacy-summary |
| [`exp-fips203-compress-unified-int-vec-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-compress-unified-int-vec-k4/) | 3377 | 向量默认 d=1；d4–11≈3382–3415 | STATUS |
| [`exp-fips203-decompress-unified-int-vec-k4`](../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-decompress-unified-int-vec-k4/) | 3215 | 已验 d=4；d=11 有验、未单记 | STATUS |

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
| [`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) | **77958** | 全链路 prefetch 默认 | SIM_BENCHMARK |
| [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-2s1e-byteencode12-vec-k4/) | 17429 | PREFETCH=1；tile32=25464 | STATUS |
| [`pass-fix-f203-alg11-12-multiplyntts-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg11-12-multiplyntts-k4/) | ~9031 | MEM_OPS=1 默认 | STATUS |
| [`pass-fix-f203-alg11-12-innerproduct-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg11-12-innerproduct-k4/) | 43992 | 4×4×1 单 AIV | STATUS |
| [`pass-fix-f203-alg11-12-innerproduct-k4-halfrows`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg11-12-innerproduct-k4-halfrows/) | 26185 | 双 AIV 半行 | STATUS |
| [`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/) | 30347 | NTT；INTT=30340 | STATUS |

---

## Alg.7 / 8 / KeyGen 子轨

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`pass-fix-f203-alg7-sample-ntt-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg7-sample-ntt-k4/) | ~80100 | 全链默认 vec_mins+672B | STATUS |
| [`pass-fix-f203-alg8-cbd-eta2-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg8-cbd-eta2-k4/) | 18048 | P2 双 AIV 默认 | STATUS |
| [`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-lines3-7-a-hat-k4/) | 381544 | 默认双 AIV 672B；单 AIV=733859 | INDEX/STATUS |
| [`pass-fix-f203-alg13-lines8-15-se-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-lines8-15-se-k4/) | 95261 | V3 默认 | STATUS |
| [`pass-fix-f203-alg13-device-keygen-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-device-keygen-k4/) | 542339 | 2 launch；STATUS 旧 886801 已过时；Cloud≈542494 | INDEX/customspec |

---

## Alg.14 / 15 PKE device

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`pass-fix-f203-alg14-lines3-15-encrypt-prep-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/) | 470502 | 单 launch prep | STATUS |
| [`pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/) | ~125000 | SIM 单 launch | qa 2026-07-07 |
| [`pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4/) | 56259 | 分组 pack | STATUS |
| [`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/) | 154781 | 1 launch compute+尾 | STATUS |
| [`pass-fix-f203-alg14-pke-encrypt-device-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-pke-encrypt-device-k4/) | **626139** | 2 launch 全链 Encrypt | STATUS |
| [`pass-fix-f203-alg15-pke-decrypt-device-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4/) | **283252** | 单 kernel | STATUS |

---

## Compress / Decompress / ByteEncode|Decode_d

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`pass-f203-compress-d-vec-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-compress-d-vec-k4/) | 3247 | 默认 d=4；d5/10/11=3121/3449/3399 | STATUS |
| [`pass-f203-decompress-d-vec-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-decompress-d-vec-k4/) | 3177 | 默认 d=4；d5/10/11=3177/3146/3184 | STATUS |
| [`pass-f203-byteencode-d-vec-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-byteencode-d-vec-k4/) | 5435 | 默认 d=4 VEC=1；d5/10/11=5537/6455/6568 | STATUS |
| [`pass-f203-alg6-bytedecode-d-vec-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-alg6-bytedecode-d-vec-k4/) | 9186 | 默认 d=4；d5/10/11=5696/6546/6641 | STATUS |
| [`pass-f203-compress-unified-int-vec-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-compress-unified-int-vec-k4/) | →exp | canonical 已迁 incubating | INDEX |
| [`pass-f203-decompress-unified-int-vec-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-decompress-unified-int-vec-k4/) | →exp | canonical 已迁 incubating | INDEX |

---

## KEM：device / stable（correctness 已冻结，不登记）

| 目录 | SIM tick | 备注 | 来源 |
|------|----------|------|------|
| [`pass-fix-f203-alg19-kem-keygen-device-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg19-kem-keygen-device-k4/) | **713227** | P1 后 3 次均值；scripts KeyGen 默认 | STATUS |
| [`pass-fix-f203-alg20-kem-encaps-device-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg20-kem-encaps-device-k4/) | **721010** | T19a pass-fix；`c`/`K` max=0；行为基线 | STATUS 2026-07-15 |
| [`pass-fix-f203-alg21-kem-decaps-device-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-k4/) | **1050923**（D**287037**+E**763886**） | Alg.21 **交付**；**T19i** SIM **3** launch；单库+1session；`DECAPS_DIR` 默认 **stable（无 `-ct`）** | STATUS 2026-07-20 |
| [`pass-fix-f203-alg21-kem-decaps-device-ct-k4`](../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-ct-k4/) | **1050461**（D**286798**+E**763663**） | Alg.21 **CT 专题**；Cloud / `SIM_DIRECT=1` / `decaps_2session`；合法 `K` max=0；拒绝 SIM≈D**286703**+E**763747**；行为基线（非 `scripts/` 默认） | STATUS 2026-07-24 |

> **2026-07-20**：`fix-f203-alg{19,20,21}-*-correctness-k4` **已冻结**（见 [`frozen/INDEX.md`](../ascendc-tests/frozen/INDEX.md)）；**勿**再跑 / 勿作回归默认。历史 tick 仅见各 `FROZEN.md`。
