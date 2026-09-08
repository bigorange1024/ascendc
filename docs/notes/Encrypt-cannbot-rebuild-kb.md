# Encrypt 重建 · 知识库（聚焦）

> **范围**：Alg.14 Encrypt + Alg.20 Encaps（设备路径）重拼所需的算法 / 工程 / 反卡死结论。  
> **排除**：设备 Decaps 全链、KeyGen 全链交付叙事、无关 profiling 长文。  
> **维护**：仅主控刷新；编码 Subagent **只读**。  
> **更新源**：capability-inventory · cannbot 审计 · 本支实验 · fe53/sticky 失败摘录。  
> **DAG**：`docs/rg-encrypt-cannbot-rebuild.yaml`  
> **状态（2026-09-08）**：**Q-ULT answered** — 设备 Encaps（含 Encrypt）NPU 权威绿；T24×30 不挂压测通过。

---

## A. 算法契约（成立）

### A1 · FIPS 203 Encrypt（ML-KEM-1024）

- 参数：\(k=4,\eta_1=\eta_2=2,d_u=11,d_v=5\)；\(c=c_1\|c_2\) 共 **1568** 字节。  
- 步骤：Decode₁₂(ek)→Â←SampleNTT→(y,e₁,e₂)←CBD(PRF)→ŷ←NTT(y)→  
  \(u=\)INTT(Âᵀ∘ŷ)+e₁ → \(v=\)INTT(⟨t̂,ŷ⟩)+e₂+μ → Compress+Encode。  
- μ：message **32B** → Decompress₁ → 域元素，再进 v 路径。

### A2 · Encaps 外包（Alg.20 设备路径）

- \(h←H(\mathsf{ek})\)；\((K\|r)←G(m\|h)\)；coins\(=r\)；Encrypt\((m,\mathsf{ek};r)→c\)。  
- 权威：设备 `(c,K)` ≡ liboqs Encaps（同 m/ek）；往返：liboqs Decaps\((\mathsf{sk},c)→K'≡K\)。  
- 证据目录：`RB-T22`（设备 G）· `RB-T23`（CROSS）· `RB-T24`（RT）。

### A3 · NTT / poly-batch（定稿）

- Stage2 后每个 AIV 握 **完整 poly 的 hi+lo**（禁 limbsplit）。  
- 三段式 NTT（S1–S3）内 **禁 Gather**。  
- 详见 `MLKEM-NTT-实现总结.md` / `MLKEM-NTT-向量与标量实现指南.md`。

### A4 · 可复用积木（本战役已接到 Encaps）

| 积木 | 本战役证据 | 备注 |
|------|------------|------|
| μ / BD₁₂ / CBD / SampleNTT / NTT(y) | T04–T05、T12–T16 NPU | G1/G2 关 |
| Encrypt 域拼装 + pack | T10–T11、T06、T22–T24 NPU | G3/G5 关 |
| MIX 1/3 + GATE(4) | T01–T03、T09、T22–T24 | 禁 5/7 |
| 设备 H/G | T22 Launch1 | Host 不预喂 K/coins |

---

## B. 工程硬约束（成立）

### B1 · Sync / CrossCore（cannbot sync-audit）

- AIC **禁止**在 `Wait` 等待环内 `SyncAll` / `IBWait` / 阻塞同步。  
- SoftSync / 自造 flag **禁止**；只用 `CrossCoreSetFlag` / `CrossCoreWaitFlag`。  
- flag：**禁 5、禁 7**；本链路仅 **1/3 复用 + 4=GATE**。  
- AIV↔AIC 握手：`Set`/`Wait` 成对；跨核写读必须有 Sync。

### B2 · 反卡死拓扑（本重建核心目的）

> **重写目的 = 实机不再卡死**；正确性是同路径附带验收。  
> **定稿展开**（对照 stable / KeyGen、检查单）：[`MIX-Encrypt-Encaps-反卡死拓扑技术总结.md`](MIX-Encrypt-Encaps-反卡死拓扑技术总结.md)（**暂行**）。

| 设计 | 作用 |
|------|------|
| **双 launch** | Launch1 AIV-only（H/G，无 CrossCore）；Host mid-sync；Launch2 MIX Encrypt。避免单 fused 核内 AIV 未 Set / AIC 死等 |
| **`BLOCK_DIM=1`** | 禁多 AIV 对半拆 Â / peer 交换（半边 Â、永远到不了 Set） |
| **flag 只 1/3+4** | 避开 5/7；**不用** stable `l18` 的 flag 2 / GATE=8 |
| **NPU 墙钟超时 + 反复压测** | hang → timeout；T24×30 `ok=30` |
| **禁抄旧树** | 不继承 `l18_l19` 同步债 |

**与 stable Encaps/Encrypt**：表面同为 2 launch，实质 L2 深 FSM（含 GATE=8）+ prep AHAT 默认 2 → 实机 **卡 SynchronizeStream** 史。  
**与 KeyGen**：同为 2 launch，但握手面短（NTT 1/2/3 + AIV SyncAll 汇合），实机主风险偏 **算错** 而非挂死 —— **不可**用 KeyGen 的 AHAT=2 反推 Encrypt 长链安全。

**压测证据**：`RB-T24` 在 910B3 连续 **30×** `-r npu` → `ok=30 fail=0`。日志：`/mnt/workspace/encrypt-rebuild-hang-stress.log`。

### B3 · 运行时 / 验收（本战役）

- **最终以 NPU 为准**（910B3 `ASCEND_DEVICE_ID=0`）；战役默认 **NPU 优先**，长 SIM 非门禁（见 `RHYTHM.md`）。  
- 权威交叉刀须远程 aarch64 `liboqs` + `liboqs_kem_ref`（见 X11）。  
- 禁并行多路 SIM；上板前清 `build/output/input`（X8）；`flock`；勿脏杀污染会话（X3）。  
- NPU 禁空等（X6/X10）；看门狗 ~30 min。

### B4 · 本重建禁令

- **禁抄** Encrypt/Encaps/Decaps 算子级实现（inventory §3）。  
- **禁**把「自洽对拍绿」当同步正确；sync_audit 红线不得否决。  
- **禁**主控直接编码；编码只经 Subagent + 任务书。  
- **禁**为「方便」改回单 launch fused / 启用 5·7 / 加大 blockDim 拆核——那是在破坏反卡死目的。

---

## C. 失败教训（优先沉淀）

> 来源：fe53 hang-rewrite · sticky-1534 · 仓内 L18 · 本战役。**现象正确 ⇒ 进表；实现代码不进表。**

### X1 · CrossCore 死锁形态

- **现象**：Host 已 launch fused 核，NPU 长时间不回；可能 D2H TRACE 全 0。  
- **常见根因类**：AIV 未到 `Set(flag)`；AIC 卡在 `Wait`；或 Wait 环内非法 Sync。  
- **本战役对策**：双 launch 拆 CrossCore 面；见 §B2。

### X2 · EARLY 空 TRACE（sticky）

- AIV0 **从未**完成首标；不是「卡在 GATE/INTT 中段」。

### X3 · 超时杀进程污染会话

- `timeout` 124 后同设备后续异常 → 须 Finalize / 清会话，勿误判卡坏。

### X4 · 假绿三问未答先改参

- golden 与实现同源？权威交叉跑了？跨核 Sync 齐？

### X5 · 勿 fork 旧 Encrypt

- fork 继承同步债；拓扑在 `graph-tests/` 新写。

### X6 / X10 · 空闲断连 / 空等杀容器

- >4 min 无操作可断；看门狗 IDLE → SIGTERM tini。NPU 轨不为等 SIM 停转。

### X7 · SIM 上 AIC/AIV1 标量 TRACE 常空

- 勿因 SIM TRACE 空槽误判握手失败（战役后期 NPU 优先后降权）。

### X8 · NPU 脏 build/output 对拍假红

- 上板前 `rm -rf build output input`。

### X9 · NPU TRACE 槽可能落在 AIV1 而非 AIV0

- verify 接受 AIV0∨AIV1；PASS 以 SynchronizeStream + causal + 产物为准。

### X11 · NPU 真机缺 aarch64 liboqs → gen_data BLOCKED

- x86 `liboqs_kem_ref` 不可搬到 aarch64；worktree 现场 `build-liboqs` + `build_liboqs_kem_ref`。  
- 库未就绪时循环 WAIT，勿 sticky 空烧。

### X12 · `GlobalTensor::SetValue` 写 GM：CPU 假绿 / NPU 错数

- **现象**（T25）：CPU `m≡liboqs`；NPU Stream 返回但 `m` 错；**未卡死**。  
- **处置**：输入直读 H2D；写出 `m`/`K`/`c` 用 UB+`DataCopy`（对齐 T22 pack）；勿依赖标量 `SetValue` 镜像 GM。  
- **铁律**：CPU 绿 ≠ NPU I/O 过。

---

## D. 成功台账（实验回写区）

| ID | 日期 | 刀 / 目录 | 结果 | 可复用结论 |
|----|------|-----------|------|------------|
| P0–P13 | 2026-09-08 | T01–T13 等 | 见 QUEUE | 积木 + 外形双 launch |
| P14 | 2026-09-08 | T22 Encaps G | CPU+NPU | 设备 (K‖r)←G |
| P15 | 2026-09-08 | T23×liboqs | CPU+NPU CROSS | X11；c/K≡liboqs |
| P16 | 2026-09-08 | T24 RT | CPU+NPU RT | Encaps→liboqs Decaps；Q-ULT |
| P17 | 2026-09-08 | T24×30 压测 | **ok=30 fail=0** | 反卡死目的实测 |
| P18 | 2026-09-08 | T25 Decrypt | CPU+NPU | 三 launch；X12 DataCopy |

路径根：`graph-tests/enc_related/RB-T*` · 运营：`graph-tests/encrypt-rebuild-ops/`。

---

## E. Subagent 只读导航

| 任务类型 | 先读 |
|----------|------|
| 任何编码刀 | inventory · KB §B · [`COMMON.md`](../../graph-tests/encrypt-rebuild-ops/COMMON.md) · 本刀 TASK |
| MIX / 反卡死 | §B1–B2 · §X1 · T22–T24 LAYOUT |
| NTT | §A3 · 定稿两篇 NTT |
| 权威交叉 | §A2 · §X11 · T23/T24 |
| 任务队列 | [`QUEUE.md`](../../graph-tests/encrypt-rebuild-ops/QUEUE.md) |

---

## F. 变更日志（短）

| 日期 | 变更 |
|------|------|
| 2026-09-08 | 初版脚手架；战役推进 T01–T24 |
| 2026-09-08 | T23/T24 双绿；X11；Q-ULT answered |
| 2026-09-08 | **收口刷新**：§B2 反卡死拓扑；P17×30 压测；Encaps 契约 §A2 |
| 2026-09-08 | 链入定稿笔记 `MIX-Encrypt-Encaps-反卡死拓扑技术总结.md`；对照 stable/KeyGen |
