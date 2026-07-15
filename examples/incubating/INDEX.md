# examples/incubating — 研究中算子
**自包含**（2026-06-29）：与探针同约束，见 [用例自包含与设备全链约束.md](../../docs/engineering/用例自包含与设备全链约束.md)；KeyGen 见各目录 `SELF_CONTAINED.md`。


**前缀**：`exp-<简述>/`（如 `exp-mlkem-ntt/`）。

**规则**（见 Rule）：研究类代码**只能先写此处**；定型后**复制**到 `examples/stable/stable-*`，本目录副本**保留**。

**废弃实验** → [../frozen/INDEX.md](../frozen/INDEX.md)（`frozen-exp-*` — **路线关闭，禁止抄码、禁止用其 customspec**）。

---

## 当前实验

| 目录 | 方案简述 | 状态 |
|------|----------|------|
| [exp-sepolyvec8-ntt-k8/](exp-sepolyvec8-ntt-k8/) | **纯 $k{=}8$ 批 NTT**（8 条互异随机 poly；交错 S0；**非** KeyGen 集成）；[PDF](exp-sepolyvec8-ntt-k8/exp-sepolyvec8-ntt-k8-实现方案.pdf) | **CPU ✓ / SIM ✓**（SIM 需 `CAMODEL_SKIP_ADX_WORK_PATH` 规避 CAModel FPE，已内置 run.sh，见 [qa 2026-07-08 §8](../../qa/2026-07/2026-07-08-Alg14-tail-pack探针.md)）；NTT 内核回归对照；[SELF_CONTAINED](exp-sepolyvec8-ntt-k8/SELF_CONTAINED.md) |
| [exp-fips203-mlkem-pke-stage1-encode-vec/](exp-fips203-mlkem-pke-stage1-encode-vec/) | F203 Stage1 纯向量 encode；[customspec](exp-fips203-mlkem-pke-stage1-encode-vec/exp-fips203-mlkem-pke-stage1-encode-vec-实现方案-customspec.pdf) | `aiv=1/2/8` 对拍 |
| [exp-fips203-mlkem-pke-stage3-routea-mod-vec/](exp-fips203-mlkem-pke-stage3-routea-mod-vec/) | F203 Stage3 RouteA+mod 向量预研；[customspec](exp-fips203-mlkem-pke-stage3-routea-mod-vec/exp-fips203-mlkem-pke-stage3-routea-mod-vec-实现方案-customspec.pdf) | `aiv=1/2/8` 对拍 |
| [exp-fips203-mlkem-pke-encrypt-k4/](exp-fips203-mlkem-pke-encrypt-k4/) | FIPS 203 **Alg.14 PKE Encrypt** k=4（ek+m+coins→**仅 c**；中间态禁落盘）；[customspec](exp-fips203-mlkem-pke-encrypt-k4/exp-fips203-mlkem-pke-encrypt-k4-实现方案-customspec.pdf) | **已晋级** [`stable-fips203-mlkem-pke-encrypt-k4`](../stable/stable-fips203-mlkem-pke-encrypt-k4/)；副本保留 · CPU/SIM/KAT/roundtrip ✓ · [STATUS](exp-fips203-mlkem-pke-encrypt-k4/STATUS.md) |
| [exp-fips203-mlkem-pke-decrypt-k4/](exp-fips203-mlkem-pke-decrypt-k4/) | FIPS 203 **Alg.15 PKE Decrypt** k=4（生产 input 仅 dk+c+lut→**仅 m**；单 kernel）；[customspec](exp-fips203-mlkem-pke-decrypt-k4/exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.pdf) | **已晋级** [`stable-fips203-mlkem-pke-decrypt-k4`](../stable/stable-fips203-mlkem-pke-decrypt-k4/)；副本保留 · CPU/SIM/KAT/roundtrip ✓ · [STATUS](exp-fips203-mlkem-pke-decrypt-k4/STATUS.md) |
| [exp-fips203-mlkem-pke-keygen-k4/](exp-fips203-mlkem-pke-keygen-k4/) | FIPS 203 **Alg.13 PKE KeyGen** k=4（**已晋级** [`stable-fips203-mlkem-pke-keygen-k4`](../stable/stable-fips203-mlkem-pke-keygen-k4/)）；[customspec](exp-fips203-mlkem-pke-keygen-k4/exp-fips203-mlkem-pke-keygen-k4-实现方案-customspec.pdf) | 副本保留；交付以 **stable** 为准 · [STATUS](exp-fips203-mlkem-pke-keygen-k4/STATUS.md) |
| [exp-fips203-mlkem-kem-keygen-k4/](exp-fips203-mlkem-kem-keygen-k4/) | FIPS 203 **Alg.19 KEM KeyGen** k=4；[customspec](exp-fips203-mlkem-kem-keygen-k4/exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf)；[STATUS](exp-fips203-mlkem-kem-keygen-k4/STATUS.md) | **已晋级** [`stable-fips203-mlkem-kem-keygen-k4`](../stable/stable-fips203-mlkem-kem-keygen-k4/)（2026-07-14）；副本保留；交付以 **stable** 为准 |
| [exp-fips203-mlkem-kem-encaps-k4/](exp-fips203-mlkem-kem-encaps-k4/) | FIPS 203 **Alg.20 KEM Encaps** k=4（prep 前段 H/G + vendored Encrypt；$m$ GM 入；$c$+$K$）；[customspec](exp-fips203-mlkem-kem-encaps-k4/exp-fips203-mlkem-kem-encaps-k4-实现方案-customspec.pdf)；[STATUS](exp-fips203-mlkem-kem-encaps-k4/STATUS.md) | **已晋级** [`stable-…-kem-encaps-k4`](../stable/stable-fips203-mlkem-kem-encaps-k4/)（2026-07-15 `#验收#`）；副本保留；tick≈**721k**；基线 [`pass-fix-…-encaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/) |
| [exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4/](exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4/) | Alg.13 行 16–20：2s1e MIX+UB；**Host Python** 提供 FIPS CBD $\mathbf{s}$/$\mathbf{e}$（	exttt{src.bin}）；ByteEncode **prefetch**；[customspec](exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4/exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4-实现方案-customspec.pdf) | **CPU ✓ / SIM ✓** tick≈**78k** |
| [exp-fips203-compress-unified-int-vec-k4/](exp-fips203-compress-unified-int-vec-k4/) | FIPS 203 **统一整数 Compress_d**（d=1/4/5/10/11；int32 limb 宽乘）；[customspec](exp-fips203-compress-unified-int-vec-k4/exp-fips203-compress-unified-int-vec-k4-实现方案-customspec.pdf) | **CPU ✓ / SIM ✓**；生产已迁入 stable Encrypt tail · [STATUS](exp-fips203-compress-unified-int-vec-k4/STATUS.md) |
| [exp-fips203-decompress-unified-int-vec-k4/](exp-fips203-decompress-unified-int-vec-k4/) | FIPS 203 **统一整数 Decompress_d**（d=1/4/5/10/11；int32 全向量）；[customspec](exp-fips203-decompress-unified-int-vec-k4/exp-fips203-decompress-unified-int-vec-k4-实现方案-customspec.pdf) | **CPU ✓ / SIM 部分**；生产已迁入 stable Decrypt unpack · [STATUS](exp-fips203-decompress-unified-int-vec-k4/STATUS.md) |

NTT 主路径：`AicMmad` + merged\_kyber FSM（非 `Matmul<>`）。**块紧凑 S0 `[HI_8|LO_8]` 已否决** → [`frozen-exp-mlkem-sepolyvec8-ntt-k4-block`](../frozen/frozen-exp-mlkem-sepolyvec8-ntt-k4-block/) + 探针 `poly8-block-s123`（**禁止参考**）。**8-poly 紧凑向量终态** → [`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../../ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/)；历史 exp → `exp-sepolyvec8-ntt-k8`；全链路 → `exp-k4` / `vec-k4-v2`。

**行 8–15 设备 $s$/$e$**：[`pass-fix-f203-alg13-lines8-15-se-k4`](../../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/)（向量 V3 ✅）；标量对照 [`frozen-fix-f203-alg13-se-device-scalar-k4`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-se-device-scalar-k4/)（2026-06-26 冻结）。

**Alg.14/15/19 交付**：[`stable-…-pke-encrypt-k4`](../stable/stable-fips203-mlkem-pke-encrypt-k4/) · [`stable-…-pke-decrypt-k4`](../stable/stable-fips203-mlkem-pke-decrypt-k4/) · [`stable-…-kem-keygen-k4`](../stable/stable-fips203-mlkem-kem-keygen-k4/)。

---

## 维护

新增 `exp-*` → 增加一行；晋级 stable 后**不删除**本行（可标「已复制至 stable-…」）。  
`Matmul<>` NTT 相关实验已迁至 `examples/frozen/` — **路线关闭，禁止抄**；见 [2026-06-11 冻结纪要](../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#ntt-matmul路线废弃冻结)。
