# MIX Decrypt/Decaps 反卡死拓扑 — 技术总结（暂行）

**读者**：后续在 AscendC 上拼 **Alg.15 Decrypt / Alg.21 Decaps** 长链的实现者与 Agent  
**目的**：说明 cannbot 重建路径为何能同时满足 **liboqs 正确** 与 **NPU 反复不挂**；给出可迁移不变量  
**案例锚点**：`graph-tests/dec_related/RB-D01`–`RB-D07`；运营 `graph-tests/decrypt-rebuild-ops/`  
**讨论**：`qa/2026-09/2026-09-09-Decrypt-Decaps-cannbot重建脚手架.md`  
**配套**：[`Decrypt-cannbot-rebuild-kb.md`](Decrypt-cannbot-rebuild-kb.md) · DAG `docs/rg-decrypt-cannbot-rebuild.yaml` · 前序 [`MIX-Encrypt-Encaps-反卡死拓扑技术总结.md`](MIX-Encrypt-Encaps-反卡死拓扑技术总结.md)

> **效力**：有证据的暂行模型，非数学证明。证据：砖级 D01–D03×30；D04/K01–K03×30；K04×30 + 补跑≈100；门禁三问均 closed。  
> **非目标**：本战役 **不** 晋级 `examples/stable-*`；**不** 修旧 alg15/21 / T25–T27；**不** 重做 PKE/KEM KeyGen。

---

## 0. 本文怎么读

| 章节 | 内容 | 依赖本仓代号 |
|------|------|--------------|
| §1 | 问题对象：Decrypt/Decaps 特有卡死面 | 否 |
| §2 | 工程不变量（继承 Encrypt + Decrypt 特化） | 否 |
| §3 | 设备拓扑模型（三核 Decrypt + 七核往返） | 少量 |
| §4 | 为何现在不挂 + KeyGen 风险对照 | 否 |
| §5 | 验证方法论与加压档位 | 否 |
| §6 | 检查单 | 否 |
| §7 | 案例路径 / 事故 | 是 |

---

## 1. 问题对象

### 1.1 Decrypt 比 Encrypt 多出来的同步面

Encrypt/Encaps 重建已证明：**短 CrossCore + 多 launch** 可避开 `l18` 式深 FSM 卡死。  
Decrypt 在此之上还有两类「半成品」风险（算法不变量，与具体 API 无关）：

1. **prep∥NTT 同 launch**：unpack 未全局可见时就开始 NTT(u) → û 半成品 / 局部 0。  
2. **NTT∥INTT 同 launch**：两段 MIX 争用同一组 flag 1–3 → 错数或死锁史。

故 Decrypt 的反卡死 = Encrypt 不变量 **加上**「按数据依赖切开 launch / kernel」。

### 1.2 Decaps 的额外面

Decaps = Decrypt + G + Re-Encrypt + FO。  
任一子段若与下一段共 session 且无 Host mid-sync，会复现历史「Decrypt 后同 session Encrypt 污染 c'」类问题。  
对策仍是：**一段一 launch（或明确边界）+ mid-sync**，禁止用「两段 `aclFinalize`」当生产基线。

---

## 2. 工程不变量（暂行）

继承 Encrypt 总结的 **I1–I5**（短跨核面、flag∈{1,3,4}、AIC Wait 环禁阻塞 Sync、`BLOCK_DIM=1`、禁 fork 旧 stable 同步模板）。Decrypt/Decaps **追加**：

### I6 · prep 与 NTT 必须 Host 屏障分隔

- Launch L1：纯 AIV prep（decode dk + unpack c）写出 ŝ,u,v。  
- Host `SynchronizeStream`（或等价全局屏障）。  
- Launch L2a：MIX NTT(u)+su_dot。  
- **禁止**把 L1/L2a 融进同一 kernel「省一次 launch」。

### I7 · NTT 与 INTT 必须分 kernel（可同二进制）

- L2a 与 L2b 分文件 / 分 launch；各自维护短 flag 面。  
- 同 binary 内 **basename 全局唯一**（`dec_prep` / `dec_ntt_dot` / `dec_intt_extract`；Encaps 侧 `enc_*`；FO `decaps_fo`），避免 auto_gen 撞名吞核（507000）。

### I8 · 业务 GM 写出走 UB+DataCopy

- 禁依赖 `GlobalTensor::SetValue` 写 m/K（CPU 可假绿、NPU 错字节）。  
- 输入宜 Host H2D 直读。

### I9 · 权威交叉与架构一致

- Golden / 交叉二进制须与 NPU **同 ISA**（aarch64）。  
- **禁止**把开发机 x86 的 `liboqs_*_ref` rsync 覆盖远端。

### I10 · 加压档位与代码成熟度匹配

| 代码性质 | 建议反复次数 |
|----------|--------------|
| 实验 / 中间（`graph-tests/dec_related` 重建脚手架） | **×30** |
| 最终算子级（`examples/stable` 等） | **×100** |

超时预算默认 **180s/次**；超时=缺陷，不是「再加大 timeout」。

### I11 · 长作业必须占心跳；刀间不得空烧

- 云机 `IDLE_MIN` 看门狗只认心跳文件；长压测期间 keepalive ≤40s。  
- 任一 NPU 作业结束后 **1 分钟内**：开下一刀 **或** 停心跳放机。  
- **禁止**门禁已绿却只留心跳空挂；也 **禁止**目标未完成时无故停手等指令。

---

## 3. 设备拓扑模型（本战役定稿）

### 3.1 Decrypt（Alg.15）

```text
L1  dec_prep_custom          AIV-only     → ŝ, u, v
    ── Host mid-sync ──
L2a dec_ntt_dot_custom       MIX 1/3(+4)  → û, ŵ
    ── Host mid-sync ──
L2b dec_intt_extract_custom  MIX 1/3(+4)  → m[32]
```

### 3.2 Decaps（Alg.21）刀序

```text
K01 设备 G(m'‖h) → (K', r')
K02 Re-Encrypt → c'（复用 Encaps 积木契约，禁抄源码）
K03 FO：合法选 K' / 拒绝 J(z‖c)
K04 设备 Encaps ↔ 设备 Decaps 往返 + 加压
```

往返七核 basename 唯一：`enc_g` / `enc_prep` / `enc_mix` / `dec_prep` / `dec_ntt_dot` / `dec_intt_extract` / `decaps_fo`。

---

## 4. 为何现在不挂 · 与未重做 KeyGen 的对照

### 4.1 重建 Decrypt/Decaps 不挂的因果（暂行）

卡死在现象上 ≈ Host `aclrtSynchronizeStream` 不回 ≈ 设备侧某 `CrossCoreWait` **永远等不到**对应 `Set`（或核未真正进入）。  
本路径相对 stable Encaps/Decrypt **同时**去掉了历史上已定位的挂死主因：

| 砍掉的挂死源 | 重建做法 |
|--------------|----------|
| 深融合 `l18_l19`（内积→GATE→INTT→pack，flag 含 2/GATE8） | **不 fork**；MIX 仅短表 **1/3(+4)** |
| prep∥NTT / NTT∥INTT 同 launch 抢握手 | **三 launch** + Host mid-sync |
| SoftSync / flag 5·7 / AIC Wait 环内 SyncAll | sync_audit 红线 |
| auto_gen 撞名吞核（507000） | 七核 basename 全局唯一 |
| Decaps 超长无屏障 session | 分段 launch + mid-sync |

压测证伪「假返回」：中间档 ×30；往返补跑 ≈100。详见 KB §B2.1。

### 4.2 未重做的 PKE / KEM KeyGen：仍有潜在卡死面吗？

**结论分级**：相对 Encaps-l18 为 **中低**；主历史故障是 **算错**，不是 SynchronizeStream 长挂——但 **不是零风险**，且本战役 **未** 给 KeyGen 做反卡死专用压测。

| | PKE KeyGen | KEM KeyGen |
|--|------------|------------|
| 骨架 | 2 launch：prep(AIV×2) → MIX compute | 同构；L2 更长（叠 KEM 尾） |
| 含 l18？ | **否** | **否** |
| 实机叙事 | 同批对照偏结果错；`probes_safe` 含 alg19 | 同左 |
| 仍可能挂 | 任意 MIX Wait 失配；AHAT=2 半写若被当成「满表」握手；同卡 l18 脏杀传染 | 同左 + L2 更长略增暴露面 |

**不能**用「KeyGen 少挂」反推 Encrypt 长链安全；也 **不能** 在未压测前宣称 KeyGen「已证明永不挂」。详 KB §B2.2 · Encrypt 反卡死 note §4.3。

---

## 5. 验证方法论

1. **权威**：liboqs（PKE Decrypt / KEM Decaps）；缺库 → BLOCKED，禁 python 冒充。  
2. **假绿三问**：golden 是否与实现同源？权威交叉是否已跑？跨核 Sync 是否齐？  
3. **真机优先**：有云 NPU 时以 `-r npu` 为写码/排错默认真相；CPU 孪生易漏 `-Werror=unused-*` 等仅 host 暴露问题。  
4. **sync_audit**：每编码刀必跑；红线不得否决。

---

## 6. 检查单（开新 Decrypt/Decaps 路径前）

- [ ] prep∥NTT 已拆 launch？NTT∥INTT 已拆 kernel？  
- [ ] flag 仅 1/3(+4)；无 SoftSync；无 5/7？  
- [ ] AIC Wait 环内无 SyncAll？`BLOCK_DIM=1`？  
- [ ] 各 `.cpp` basename 唯一？  
- [ ] m/K 写出为 DataCopy？  
- [ ] 远端 liboqs ref 为 aarch64？  
- [ ] 加压次数与代码档位匹配（中间×30 / 算子×100）？  
- [ ] 长跑 keepalive 开着；刀间有下一刀或已放机？

---

## 7. 案例附录（本仓路径）

| 刀 | 目录 | 关闭证据 |
|----|------|----------|
| D01–D03 | `RB-D01`…`D03` | CPU + NPU 回填 |
| D04 | `RB-D04-decrypt-full` | m≡liboqs；Q-DEC |
| K01–K03 | `RB-D04-decaps-G`…`RB-D06` | Q-DECAPS |
| K04 | `RB-D07-enc-decaps-rt` | ×30 + 补跑≈100；Q-RT-HANG |

### 7.1 本战役事故（现象正确 ⇒ 约束）

| ID | 现象 | 约束 |
|----|------|------|
| X17 | NPU host `-Werror=unused-function`（`AllocSz` 仅 CPU） | 仅 CPU 路径符号包进 `#ifdef ASCENDC_CPU_DEBUG`；有卡勿只在 CPU 上「写通」 |
| X18 | rsync 本机 x86 `liboqs_*_ref` → Exec format error | 远端只编/拷 aarch64；rsync `scripts/` **排除** 这些二进制 |
| X19 | 中间用例空压 ~1.5h / 心跳空挂烧卡费 | I10 + I11 |
| X20 | 长压测中途云机自断 | 作业内持续 touch 心跳；勿假设 IDLE_MIN 够长 |

---

**状态**：Decrypt/Decaps cannbot 重建门禁已关；本总结为原理沉淀（2026-09-09）。
