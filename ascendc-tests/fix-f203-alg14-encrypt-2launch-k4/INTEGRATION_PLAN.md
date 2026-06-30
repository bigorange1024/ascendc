# INTEGRATION_PLAN — fix-f203-alg14-encrypt-2launch-k4

**定位**：FIPS 203 **Algorithm 14（ML-KEM-1024 PKE.Encrypt，k=4）** 的 AscendC **设备全链**正确性探针，
**从零按 keygen 蓝本重建**——核心目标是 **单 ACL session + 少量 MIX launch** 的设备编排，
彻底规避旧探针 [`fix-f203-alg14-pke-encrypt-correctness-k4`](../fix-f203-alg14-pke-encrypt-correctness-k4/)
的 SIM 顽疾（多段 `aclInit/aclFinalize` + NTT 后再 launch AIV-only 核 → `507000` + `free(): invalid pointer`）。

> **正确性优先，性能其次**；禁止 liboqs / 外部 KEM 黑盒；golden 仅作 I/O oracle，不作设备实现规格。

**主蓝本（只读参考，活跃探针）**：[`pass-fix-f203-alg13-device-keygen-k4`](../pass-fix-f203-alg13-device-keygen-k4/)
（prep `AIV_ONLY` + compute `MIX_AIC_1_2`，2 launch 单 session，SIM ✅ + liboqs KAT ✅）。

**计算资产来源（活跃，可 vendored 复制）**：旧 encrypt 探针的各计算核（CPU 全链已 PASS）+ keygen + stage123/innerproduct/compress-d/byteencode-d/alg7/cbd-eta2。

---

## 0. 为什么重建（病根 → 解法）

| 旧探针病根 | 证据 | 本探针解法 |
|------------|------|------------|
| **多段 ACL session**（G1/G2/G3 各 `aclInit/aclFinalize`） | `G3_SIM_AUDIT.md` §3.2、§9.6；SIM wall ~426s | **全链单一 session**（一次 `aclInit`/`aclFinalize`） |
| **NTT(MIX) 之后单独 launch AIV-only 核**（g4 noise/pack） | `507000`；`device_aiv.o` 在 MIX binary 后注册失效 | **NTT 之后的每个 launch 都是 MIX 核**（真 MIX 或单文件 MIX、AIC 分支空跑） |
| 五参 `g3_linear` / `t_dot_r` 入口在 SIM `launch 即 507000` | `G3_SIM_AUDIT.md` §7、§8 | 内积核改 keygen 同源 `MIX` 入口（参 `mmad_custom` AIV 分支 basemul） |
| 末尾 `free(): invalid pointer` | 多次 Init/Finalize 生命周期 | 单 session + 单次 GM 分配/释放 |

**关键事实（已核实，2026-06-30）**：MIX kernel **未被冻结**——活跃 keygen `mmad_custom` 即
`MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）`，SIM ✅。被冻结的是 `onelaunch`（`SHAT_PEER`
AIV↔AIV CrossCore peer）、NTT-内-`Matmul<>`、`merged_kyber` 等**具体编排**，非 MIX 机制。

---

## 1. 数学契约（FIPS 203 Alg.14，k=4）

```text
input:  ek_PKE (1568B) = ByteEncode₁₂(t̂[4]) ‖ ρ[32]
        m (32B)
        coins (32B) → PRF 得 r[4](η₁), e₁[4](η₂), e₂(η₂)
output: c (1568B) = c₁ ‖ c₂
        c₁ = ‖_{p<4} ByteEncode₁₁(Compress₁₁(u[p]))   (4×352B = 1408B)
        c₂ = ByteEncode₅(Compress₅(v))                 (160B)
```

| 步 | 公式 | 域 |
|----|------|----|
| 采样 | ρ→Â[4,4]（Alg.7 SampleNTT ×16）；coins→r,e₁,e₂（Alg.8 CBD η₁/η₂） | — |
| 行16 | r̂ = NTT(r) | 时域→NTT |
| 行18 | û[p] = Σ_j Â[j,p]∘r̂[j]（**Âᵀ·r̂**，读 `a_hat_offset(j,p)`）；tr̂ = Σ_j t̂[j]∘r̂[j] | NTT 域 basemul |
| 行19 | u = INTT(û) + e₁ | NTT→时域 |
| 行20 | v = INTT(tr̂) + e₂ + Decompress₁(m) | NTT→时域 |
| 行21–22 | c₁=ByteEncode₁₁(Compress₁₁(u))；c₂=ByteEncode₅(Compress₅(v)) | 压缩打包 |

**与 KeyGen 的差异**：噪声 η₁/η₂（非 η,η,η）；矩阵乘读 `A[j,p]`（转置，同一 GM）；输出 Compress+ByteEncode d=11/5（非 ByteEncode₁₂）；多一轮 INTT。

---

## 2. Launch 编排（单 session；调试期分段、性能期可融合）

NTT 后所有 launch 必为 **MIX**（核类型约束见 §3）。调试期按 Stage Gate 分段，便于逐段对拍：

```text
ACL session 开始（单次 aclInit）
 GM 分配：ek, m, coins (H2D) | a_hat, t_hat, r/e1/e2, r_hat, u_hat, tr_hat, u, v, c, ws (device)

 L1  prep        AIV_ONLY      ρ→â(Alg.7×16) | coins→r,e₁,e₂(CBD) | ek→t̂(ByteDecode₁₂)
 L2  ntt_r       MIX_AIC_1_2   r̂ = NTT(r)               [AIC: limb6 MMAD; AIV: split/pack/merge]
 L3  linear      MIX_AIC_1_2   û=Âᵀ·r̂、tr̂=t̂·r̂          [AIV: Alg.11 basemul 内积; AIC: 空跑/参与]
 L4  intt_noise  MIX_AIC_1_2   u=INTT(û)+e₁、v=INTT(tr̂)+e₂+Decompress₁(m)  [AIC: INTT MMAD; AIV: +noise/μ]
 L5  pack        MIX_AIC_1_2   c₁=ByteEncode₁₁(Compress₁₁(u))、c₂=ByteEncode₅(Compress₅(v))  [AIV; AIC 空跑]

 c (D2H) → output/c.bin
ACL session 结束（单次 aclFinalize）
```

**融合路线（性能阶段，正确性达标后）**：L2+L3 融合为一个 MIX（即 keygen `mmad_custom` 的 NTT+内积模式）；
L4+L5 融合为 INTT+pack。目标 ≤3–4 launch。**当前阶段不追求融合**。

---

## 3. 核类型约束（本探针铁律）

| 约束 | 理由 |
|------|------|
| **L1 prep = `AIV_ONLY`，且必须最先 launch** | `device_aiv.o` 须在任何 MIX binary 之前注册（keygen 同序：prep AIV → compute MIX） |
| **L2–L5（NTT 及之后）= `MIX_AIC_1_2`** | AIV-only 核排在 MIX 之后 launch → SIM `507000`（旧探针实证） |
| **每个 MIX 核单独一个 `.cpp` 文件，单 kernel** | `ascendc auto_gen` 对「单文件多 MIX kernel」会降级为 `K_TYPE_AIV`（旧探针 `g3_linear.cpp` 实证）；`decode_t_hat`（单文件单 MIX）则被正确标 `K_TYPE_MIX_AIC_MAIN` |
| **纯向量段（linear/pack）的 AIC 分支正确空跑** | 参 keygen `mmad_custom`：`if (AIC) { ...; return; }`；空跑分支不得遗留未配对 CrossCore flag |
| **CPU 路径用 `#ifdef ASCENDC_CPU_DEBUG` 走 `AIV_ONLY`** | CPU tikicpu 对 MIX 拓扑表现不同；Twin Path：CPU 标量语义、SIM 向量性能，共用同一 golden |

---

## 4. Stage Gate（每 Gate 只新增一个未知量；先 Python golden 后设备对拍）

| Gate | 设备 | 新增未知量 | 对拍 golden |
|------|------|------------|-------------|
| G0 | L1+L2…L5 全 marker 壳 | launch/session 框架 | kernel 正常结束、无 507000/无 stray dump |
| G1 | L1 prep | 采样/decode | `r,e₁,e₂,â,t̂` vs `gate_g1` |
| G2 | +L2 | NTT | `r̂` vs `gate_g2` |
| G3 | +L3 | 内积 | `û,tr̂` vs `gate_g3` |
| G4 | +L4 | INTT+噪声+μ | `u,v` vs golden |
| G5 | +L5 | Compress+pack | `c.bin` 1568B `max=0` vs `golden_c` |

**方法论**（依 `docs/notes/F203-2s1e-NTT内积UB融合技术总结.md` §4）：每 Gate 结束 dump 中间态到 GM
供 host head 对拍（Early Probe）；**禁止**在生产者-消费者之间插非权威 GM 读路径；CPU/SIM 双模式验收。

---

## 5. 验收（双模式，对齐仓库强制门禁）

```bash
cd ascendc-tests/fix-f203-alg14-encrypt-2launch-k4
bash run.sh -r cpu -v Ascend910B4              # 全链 CPU，c.bin max=0
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4 # 全链 SIM，c.bin max=0；dump 仅 OPPROF_*/dump/
```

kernel 计算 ≤15s（`KERNEL_COMPUTE_BUDGET_SEC`）；SIM 须 `camodel_sim_log` 接入、无用例根 stray dump。

---

## 6. 自包含

源码仅本目录 + `library/shared`（编译期）；禁止 `#include` 其它探针/example 路径；
golden 仅本目录 `scripts/host_golden/`（C/Python ref，禁 liboqs）。详见 [`SELF_CONTAINED.md`](SELF_CONTAINED.md)。
