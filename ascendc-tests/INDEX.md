# ascendc-tests — AscendC 平台功能探针

| 规则 | 说明 |
|------|------|
| **目录名** | `<简述>/`，不加 `exp-` 前缀 |
| **`pass-` 前缀** | CPU + SIM 均已验收的**终态**探针加 `pass-`；进行中优化等保持 `fix-` |
| **状态** | 各目录 `STATUS.md`：`CPU` / `SIM` 列 |
| **文档** | 策略与纪要 → `docs/`、`qa/` |
| **废弃** | `frozen/frozen-*/` — **路线关闭**；见 [frozen/INDEX.md](frozen/INDEX.md)；禁止抄 frozen 源码与路线 |
| **SE 采样 golden** | [`library/shared/fips203_se_sample/golden_se_sampling.py`](../library/shared/fips203_se_sample/golden_se_sampling.py) |

**共用设备原语**（`library/shared/`）：SHAKE/Keccak AscendC、`fips203_se_sample`；见 [`../library/shared/INDEX.md`](../library/shared/INDEX.md)。

**自包含与设备全链**（2026-06-29）：活跃探针/example **不得**跨目录引用源码（`library/shared` 与仓库 `scripts/` CANN 壳除外）；KeyGen 生产路径禁止 Host 辅助密码计算。见 [用例自包含与设备全链约束.md](../docs/engineering/用例自包含与设备全链约束.md)。

**多环境 `run.sh`**（2026-07-14）：活跃探针已 `source` [`scripts/runtime_env.sh`](../scripts/runtime_env.sh)（`-r auto|verify`、WSL 禁 npu）；见 [NPU真机环境说明.md](../docs/engineering/NPU真机环境说明.md)。`frozen/` **不接入**。

---

## 目录结构（2026-07-27）

```text
ascendc-tests/
├── add_custom/                 # 冒烟（非算法）
├── pass-shake*/ pass-toy*/ …   # 平台玩具 / 授权示例（非 ML-KEM 参数组树）
├── ml-kem/
│   ├── ml-kem-1024/            # FIPS 203 ML-KEM-1024（k=4）活跃探针
│   ├── ml-kem-768/             # ML-KEM-768（k=3）；W0+W1+W2+W3 已绿（含 D21ct）
│   └── ml-kem-512/             # ML-KEM-512（k=2）；P2 W0–W3 全绿；W4+glue 见 incubating
│       └── INDEX.md
├── frozen/                     # 已关闭路线（不迁入 ml-kem/）
└── INDEX.md                    # 本文件
```

| 路径 | 角色 |
|------|------|
| [ml-kem/](ml-kem/INDEX.md) | **按参数组**组织的 ML-KEM 探针；详表见各 `ml-kem-*/INDEX.md` |
| [ml-kem/ml-kem-1024/](ml-kem/ml-kem-1024/INDEX.md) | **当前** ML-KEM-1024（k=4）活跃探针全集 |
| [ml-kem/ml-kem-768/](ml-kem/ml-kem-768/INDEX.md) | **W0–W3 全绿** ML-KEM-768（k=3）；incubating W4+glue 已完成；见参数卡 |
| [ml-kem/ml-kem-512/](ml-kem/ml-kem-512/INDEX.md) | **P2 W0–W3 全绿** ML-KEM-512（k=2）；incubating W4+glue 有条件完成；禁 stable-512 |
| [frozen/](frozen/INDEX.md) | 路线关闭；只读判决书 |

---

## 非 ML-KEM 探针（本层）

| 目录 | 简述 | CPU | SIM |
|------|------|-----|-----|
| [add_custom/](add_custom/) | 向量加法冒烟 | — | — |
| [**pass-shake128-ops-math-toy/**](pass-shake128-ops-math-toy/) | 共享核 **SHAKE128**（`*_toy_ub.hpp` 全 UB 参考） | ✓ | **12285** |
| [**pass-shake256-ascendc-toy/**](pass-shake256-ascendc-toy/) | 共享 `shake_xof_kernel` + **SHAKE256**（全 UB toy） | ✓ | **12285** |
| [**pass-toy-mix-s123-byteencode-k2/**](pass-toy-mix-s123-byteencode-k2/) | **MIX 玩具**：双 AIV S1 → Cube 64³ → UB Adds+func1（无跨 AIV） | ✓ | ✓ |
| [**fix-encrypt-skel-mix-chain-toy/**](fix-encrypt-skel-mix-chain-toy/) | **Encrypt 骨架 toy**：skipNtt Wait(4)；`SKEL_HOST_MU` Host 折 μ / 设备 μ-stub；OMIT_SET4 挂 | — | **✓** TASK-005：HOST_MU=0/1 绿；OMIT **124** |
| [**fix-decrypt-skel-mix-chain-toy/**](fix-decrypt-skel-mix-chain-toy/) | **Decrypt fused 握手 toy**：SoftSync+两轮 GATE+stub Cube；magic `SKELDEC1`/`0x04`；`SKEL_OMIT_SET4=1`⇒**124** | — | **✓** TASK-009：A wall≈3.9s；B budget60 **124** |
| [**fix-encrypt-clean-hostmu-2launch/**](fix-encrypt-clean-hostmu-2launch/) | **干净 Encrypt P0**：Host 2-launch + **结构默认** Host μ；L2 skipNtt **无 PrefixEmbed**；Wait(4)↔SET(4) | — | **✓** TASK-007：magic `CLNENC01`/`0x21`；wall≈4.7s |
| [**pass-merged-kyber-mix-ntt256/**](pass-merged-kyber-mix-ntt256/) | **授权示例**：Kyber 单 poly n=256 MIX NTT；**非** FIPS Tag5T；SIM **10348** tick | ✓ | ✓ |

---

## ML-KEM 活跃探针

**入口** → [**ml-kem/ml-kem-1024/INDEX.md**](ml-kem/ml-kem-1024/INDEX.md)。

摘要：

| 主题 | 目录（均在 `ml-kem/ml-kem-1024/`） |
|------|----------------------------------|
| NTT 向量基线 | `pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2` |
| PKE KeyGen / Encrypt / Decrypt device | `pass-fix-f203-alg13-device-keygen-k4` · `…-alg14-pke-encrypt-device-k4` · `…-alg15-pke-decrypt-device-k4` |
| KEM device | `…-alg19-kem-keygen-device-k4` · `…-alg20-kem-encaps-device-k4` · `…-alg21-kem-decaps-device-k4`（交付）· `…-decaps-device-ct-k4`（CT 专题） |

对应交付：[`examples/stable/ml-kem/ml-kem-1024/`](../examples/stable/ml-kem/ml-kem-1024/)；预研：[`examples/incubating/ml-kem/ml-kem-1024/`](../examples/incubating/ml-kem/ml-kem-1024/)。

**KEM `scripts/` 默认**：KeyGen→`pass-fix-…-keygen-device-k4`；Encaps/Decaps→对应 **stable**（可覆盖回 pass-fix）。**禁止**默认链指已冻结的 `*-correctness-k4`。

~~`fix-f203-alg19/20/21-*-correctness-k4`~~ — **2026-07-20 冻结** → [`frozen/INDEX.md`](frozen/INDEX.md)。根目录若残留同名空壳/`build_*` 产物，按 [`scripts/cleanup-ascendc-test-ghosts.sh`](../scripts/cleanup-ascendc-test-ghosts.sh) 清理；**不是**活跃探针。

---

## 已关闭路线（frozen）

见 [frozen/INDEX.md](frozen/INDEX.md)。**只读 `FROZEN.md`**；禁止抄 frozen 实现。活跃 MLKEM 向量见 `ml-kem/ml-kem-1024/` **vec-k4-v2**。
