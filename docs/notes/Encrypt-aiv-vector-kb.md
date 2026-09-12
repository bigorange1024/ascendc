# Encrypt/Encaps · 单 AIV · 全向量 · 知识库

> **核心问题**：ML-KEM-1024 在 **单 AIV、单 launch、底层全向量** 下实现 Encrypt + Encaps，SIM 与 liboqs 交叉正确。  
> **切割**：与 `Encrypt-cann-ntt-kb`（MIX/Cube 多 launch）正交；禁抄其核。  
> **战役**：`graph-tests/aiv-kem-vector-sim/` · 图谱 `docs/rg-encrypt-aiv-vector.yaml`

## 0. 锁定（2026-09-12 用户）

| ID | 事实 |
|----|------|
| L1 | 参数 = **ML-KEM-1024**（k=4,n=256,q=3329,du=11,dv=5,η=2） |
| L2 | **单 AIV = 一核**；一核可跑完整单用例 |
| L3 | **单用例单 launch**；禁 Host 拆 NTT/matvec/pack 多 launch |
| L4 | **禁 Cube / 禁 CrossCore / 禁 GATE 胖 MIX** |
| L5 | 底层能力（NTT/SHA3/内积/编解码/压缩）**至少用到 Vector** |
| L6 | 验收 = SIM cpu+`SIM_DIRECT=1`；对拍 liboqs；**禁擅自分支/push** |
| L7 | 主控设计+图谱/KB；**subagent 编码**；强制 cannbot-skills（尤其 sync_audit） |
| L8 | **全 AscendC**：Â SampleNTT、t̂ ByteDecode、CBD、FO、NTT/变换均须设备同 launch；Host 不得预喂密码学中间态（2026-09-12 加锁） |

## 1. 架构不变量

| ID | 不变量 |
|----|--------|
| A1 | `KERNEL_TYPE_AIV_ONLY`，blockDim=1（单用例） |
| A2 | NTT 积木基线 = AV01（Gather 蝶形）；INTT 须同族向量实现（源包无 INTT，本线新建） |
| A3 | 禁止整段纯标量扫 256 系数当主路径 |
| A4 | Encaps = FO 头（H/G）+ Encrypt；哈希优先设备向量 Keccak，允许分刀后置并入同 launch |
| A5 | 多核并行 = 多任务；用例间无 Sync（本战役先单用例） |
| A6 | **扩展语义（用户确认）**：单 AIV 可串行多用例；无需组件间同步；有多少 AIV 即可并行多少用例 |

## 1.1 相对 cann-ntt 的选型理由

| 维度 | 本线（AIV-only） | cann-ntt（MIX+Cube） |
|------|------------------|----------------------|
| 同步 | 核内闭环，无 AIC↔AIV | 多段/Cube，组件同步成本高 |
| 并行 | 1 AIV↔1 任务自然映射 | 批摊销强，跨核编排重 |
| 输入契约 | 全设备：`ek\|m(\|coins)` | EN13 仍有 Host Prep 中间态 |
| SIM tick（Encrypt） | ≈1.89M（全设备） | ≈0.81M（更快但非同等全设备） |

资产入口：[`graph-tests/aiv-kem-vector-sim/ASSETS.md`](../../graph-tests/aiv-kem-vector-sim/ASSETS.md)

## 2. 禁令（继承 + 本线）

| ID | 禁令 |
|----|------|
| B1 | 抄 `enc_cann_ntt` / examples encrypt 全核 |
| B2 | 引入 Cube / CrossCore flag 4/5/7/8 |
| B3 | 无 sync_audit 声称通过 |
| B4 | 假绿：golden 与实现同源且无权威交叉 |
| B5 | git checkout -b / commit / push 未经用户当次授权 |

## 3. 积木来源（只读契约 / 可迁向量实现）

| 能力 | 来源 | 本线动作 |
|------|------|----------|
| 正向 NTT | `graph-tests/aiv_ntt/AV01-*` | 迁入并扩 polyvec |
| INTT | 无现成 AIV 包 | **新建**向量 INTT（互逆根） |
| Keccak/SHA3 | `library/shared/keccak_f1600_kernel` | 设备向量封装 |
| ByteDecode12 | `library/shared/f203_byte_codec` / RB-T05 | 向量路径 |
| Compress/Encode | F203 compress/byteencode 探针笔记 | 向量路径 |
| golden/oracle | `f203_kem_ref` + liboqs | Host 交叉 |

## 4. 台账（随刀追加）

| 日期 | 刀 | 结果 | 笔记 |
|------|-----|------|------|
| 2026-09-12 | W0 | 本文件+图谱+QUEUE 落盘 | 开线 |
| 2026-09-12 | AE-E2 | **PASS** cpu+SIM c≡liboqs max=0；tick≈1.19M | HOST_SAMPLE；INTT 新建；sync 红线 0 |
| 2026-09-12 | AE-P2强 | **PASS**（主控复验） | DEVICE_FO+DEVICE_CBD；c/K max=0；SIM tick≈1384945；Â/t̂仍HOST |
| 2026-09-12 | AE-P2 | **PASS 强** DEVICE_FO+设备 CBD；c/K≡liboqs；tick≈1.39M | Â/t̂ SampleNTT 仍 Host |
| 2026-09-12 | 再验 | AE-E SIM tick≈**1200017**；AE-P SIM tick≈**1384867**；双端 c/K max=0 | 无代码改动；禁分支/push |
| 2026-09-12 | L8加锁 | 用户：必须全部 AscendC；旧 HOST_SAMPLE 完成口径作废 | 下发 TASK-AE-FULL-ASCENDC；编码 subagent WIP |
| 2026-09-12 | AE-FULL | **PASS**（主控复验） | 全 AscendC；AE-E tick≈1885458；AE-P tick≈1977711；c/K max=0；Host 仅 ek\|m(\|coins)+表 |
