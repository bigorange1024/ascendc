# F203 KEM Alg.19 KeyGen 设备全链 — 技术总结

**读者**：未参与本仓库开发的实现者 / Agent  
**目的**：说明 FIPS 203 **Algorithm 19** `ML-KEM.KeyGen()` 在 **ml_kem_1024（k=4）** 上的**随机性契约**、经 **Alg.16 `KeyGen_internal`** 的拼装增量、**I/O 契约**与**设备全链不变量**  
**案例锚点**：

- **交付 / 设备主线**：[`stable-…-kem-keygen-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4/) · [`pass-fix-f203-alg19-kem-keygen-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg19-kem-keygen-device-k4/)（CPU+SIM PASS；SIM ~713k tick）
- **历史 correctness oracle**：**已冻结**（2026-07-20）— 只读 [`FROZEN.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg19-kem-keygen-correctness-k4/FROZEN.md)；**禁止**翻 frozen 源码

**讨论**：[`qa/2026-07/2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md`](../../qa/2026-07/2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md)  
**实现方案**：device [`INTEGRATION_PLAN.md`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg19-kem-keygen-device-k4/INTEGRATION_PLAN.md)（correctness 计划书已随冻结归档，勿再作实现依据）

---

## 0. 本文怎么读

| 章节 | 内容 | 是否依赖本仓库代码名 |
|------|------|----------------------|
| §1 | FIPS 代数与 liboqs I/O | 否 |
| §2 | 设备全链不变量 | 否 |
| §3 | PKE 复用与 vendor 治理 | 少量 |
| §4 | SHA3 设备分层（可替换后端） | 少量 |
| §5 | 验证方法论 | 否 |
| §6 | 案例对照 | 是 |

---

## 1. 数学与数据契约

### 1.1 参数集

**ml_kem_1024**：\(n=256\)，\(q=3329\)，\(k=4\)。与 PKE KeyGen / Encrypt / Decrypt 探针同一参数集；文档中勿与历史笔误「768」混用。

### 1.2 Algorithm 16 internal（给定 `d`, `z`）

PKE KeyGen（Alg.13）由 `d` 驱动；ML-KEM.KeyGen_internal 在 PKE 输出上追加 `H(ek)` 与 `z`：

```text
(ek_PKE, dk_PKE) ← K-PKE.KeyGen(d)
ek  ← ek_PKE
dk  ← dk_PKE || H(ek) || z
```

### 1.3 Algorithm 19（本探针生产路径）

```text
d ←$ B^32    // device AscendC，UB 驻留，不导出
z ←$ B^32    // device AscendC，UB 驻留，不导出
(ek, dk) ← ML-KEM.KeyGen_internal(d, z)
```

**用户锁定（2026-07-01）**：`d`/`z` 的生成与中间计算**不得导出保存**；首版在 **UB** 完成（复用 stable `DerandFromSeedD` 模式 + 新增 `DerandZFromSeedD`）。可复现验收时 Host 仅提供 `seed_d.bin`（4B），**不**提供 64B `kem_seed`。

### 1.4 liboqs 展开秘密钥（本仓 I/O 锁定）

与 `OQS_KEM_ml_kem_1024` 对拍时，秘密钥为 **3168 字节**：

```text
dk_kem = dk_pke (1536) || ek (1568) || H(ek) (32) || z (32)
ek_kem = ek_PKE (1568)
```

代数最小形态 `dk_PKE || H(ek) || z`（1600B）是 FIPS 行级描述；**实现验收以 liboqs 3168B 布局为准**（与 [`scripts/liboqs_pke_ref_mlkem1024.c`](../../scripts/liboqs_pke_ref_mlkem1024.c) 中 `KEM_SK_BYTES` 一致）。

---

## 2. 工程不变量

| 不变量 | 说明 |
|--------|------|
| **设备全链** | `H(ek)`、采 `z`、拼接 `dk_kem` 均在 AI Core 完成；Host 只写 `seed_d`、读 output、`VERIFY=1` 对拍 |
| **Alg.19 `d`/`z`** | **均在 device AscendC 生成**；UB 驻留；**禁止** D2H/落盘 `d`/`z` 本体 |
| **无 Host 胶水** | 禁止 Host `tiny_sha3` / liboqs 参与默认 `run.sh` 生产路径 |
| **单进程 launch** | 禁止子进程调 stable / 其它探针 `run.sh`；单 ACL session 内 vendor PKE + KEM 尾段 |
| **自包含** | PKE 能力 **vendor 复制**到本探针目录；仅可 `#include` `library/shared/` |
| **Golden 角色** | host golden / liboqs 仅 oracle；不作 AscendC 实现规格 |

---

## 3. PKE 复用模型

Alg.19 KeyGen **不重写** NTT、内积、ByteEncode₁₂ 等 PKE 设备核（经 KeyGen_internal 调用 Alg.13）。

| 角色 | 路径 | 用法 |
|------|------|------|
| **权威实现** | `examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-keygen-k4` | vendor 源；liboqs ek/dk_pke 已验 |
| **调试对照** | `ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-device-keygen-k4` | 非生产 `#include` 源 |

**vendor 治理**：`vendor/pke_keygen/` 由 stable 同步复制；G1 Gate 要求同 `SEED_D` 下 `ek_pke`/`dk_pke` 与 stable 输出 max=0。

> **对照 Encaps/Decaps（2026-07-10）**：Alg.19 已对齐 stable KeyGen；Alg.20/21 仍 vendor frozen Encrypt/Decrypt G5/G4（布局债）。统一收口见 [qa/TODO.md](../../qa/TODO.md) **T19**（本探针复核为 **T19d**）。

---

## 4. SHA3 设备分层（可替换后端）

用户约束（2026-07-01）：

1. 密码学哈希**留在 AscendC 工程内、在 device 上执行**。  
2. 对外暴露稳定设备 API（如 `F203SeDeviceKeccak::Sha3OneShot`）。  
3. **当前**内部可实现为标量 Keccak-f[1600]（语义对齐 `thirdparty/tiny_sha3`）。  
4. **未来** CANN 矢量 SHA3 就绪后，**只替换 backend 实现**，调用方不变。

本探针首版主要用 **单次 SHA3-256**（`H(ek)`）；`d`/`z` 派生同样走 `Sha3OneShot`（与 stable `DerandFromSeedD` 一致）。若 `z` 需 XOF 形态，可走 `Shake256OneShot` + domain sep，但**输出仍只留在 UB**直至写入 `dk_kem`。

Host `tiny_sha3` **仅**用于 `scripts/host_golden/` 与仓库级 `liboqs_kem_vs_ascendc`（待建）。

---

## 5. 验证方法论

| 层级 | 手段 | 证明什么 |
|------|------|----------|
| G1 | vendor PKE 段 vs stable | PKE 段未写坏 |
| G2 | `H(ek)`、`z` 中间张量 vs host golden | KEM 增量正确 |
| G3 | `ek_kem`/`dk_kem` 端到端 | 探针自洽 |
| L2 | `liboqs_kem_vs_ascendc.sh`（待建） | 与标准实现字节一致 |
| CPU+SIM | 双模式 `run.sh` | 同步 / 搬运 / func_key |

固定种子 **`SEED_D=20260619`** 与 PKE liboqs 三阶段同源，便于横向对比。

---

## 6. 案例对照（附录）

| 项 | 历史 correctness（已冻结） | device（主线） |
|----|---------------------------|----------------|
| 探针目录 | 只读 [`FROZEN.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg19-kem-keygen-correctness-k4/FROZEN.md) | `ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg19-kem-keygen-device-k4` |
| Launch | 3（vendor PKE 拼装；**勿再跑**） | **2**（stable PKE + mmad 内嵌 Alg.16 尾） |
| 状态 | **已冻结**（2026-07-20） | **PASS**（2026-07-10） |
| SIM tick | 历史 ~742k | ~713k（P1 后均值） |
| 脚本默认 | **禁止** | **KeyGen**（`roundtrip_kem_*` 等） |

后继：**T19a** Alg.20 Encaps device [`pass-fix-f203-alg20-kem-encaps-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg20-kem-encaps-device-k4/) · Alg.21 Decaps device。

---

## 7. 可复用模式

1. **KEM = PKE 积木 + Alg.19 双随机 UB 尾**：`d`/`z` 设备生成不导出；大算力段 vendor PKE；尾段 SHA3 + 拼接。  
2. **liboqs I/O 优先于 FIPS 最小 dk**：展开 `dk_pke‖ek‖H‖z` 避免与生态工具链尺寸不一致。  
3. **SHA3 门面 + 可换 backend**：设备路径永不回退 Host；`d`/`z`/`H(ek)` 同门面。
