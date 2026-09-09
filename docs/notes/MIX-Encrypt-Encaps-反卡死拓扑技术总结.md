# MIX Encrypt/Encaps 反卡死拓扑 — 案例附录（暂行）

> **写码请先读唯一 KB**：[`ascendc-engineering-kb.md`](ascendc-engineering-kb.md) + [`rg-ascendc-engineering.yaml`](../rg-ascendc-engineering.yaml)。  
> **本文角色**：Encrypt/Encaps 重建的**拓扑案例与检查单展开**；刀号 / ×30 / 三路对照属附录。  
> 与工程 KB 冲突时 **以 KB + 机读图为准**（2026-09-09 起）。

**读者**：需要对照重建 vs stable 拓扑细节时  
**案例锚点**：`graph-tests/enc_related/RB-T22`–`RB-T24`；对照 `stable-*-kem-encaps-k4` / `pass-fix-*-alg20|alg14`  
**讨论**：`qa/2026-09/2026-09-08-Encrypt-cannbot重建脚手架.md`  
**运营**：`graph-tests/encrypt-rebuild-ops/`（非知识）

> **效力**：有证据的暂行案例，不是数学证明。修订机制/决策请改工程 KB 与 yaml，再视需要回写本节。

---

## 0. 本文怎么读

| 章节 | 内容 | 是否依赖本仓代号 |
|------|------|------------------|
| §1 | 问题对象：卡死 vs 算错 | 否 |
| §2 | 工程不变量（反卡死） | 否 |
| §3 | AscendC / CrossCore 落地 | 少量 |
| §4 | 三路对照（重建 / stable Encaps·Encrypt / KeyGen） | 是（附录） |
| §5 | 指导后续开发的检查单 | 否 |
| §6 | 案例路径 | 是 |

---

## 1. 问题对象

### 1.1 两类失败勿混

| 类型 | 现象 | 典型算子叙事 |
|------|------|--------------|
| **卡死** | Host `aclrtSynchronizeStream` 长时间不回；常需 timeout/Ctrl+C | stable **Encaps/Decaps** 卡在 `l18_l19` |
| **算错** | Stream 返回但对拍失败 | 同批实机上 **KeyGen** 曾表现为结果不对 |

重写 Encrypt/Encaps 的**首要目的**是消灭卡死类失败；正确性（liboqs）是同路径附带门禁。

### 1.2 卡死的最小机制模型

跨核 MIX 上，挂死 ≈ **某侧永远等不到对端的 Set**：

1. AIV 卡在前置算子（采样 / 编码 / 半边数据）→ **从未 Set(flag)**；  
2. AIC 已在 **Wait(flag)**（或 Wait 环内又做了阻塞 Sync）→ 双端死锁；  
3. Host 只看见 Stream 不回。

因此「不挂」首先靠 **缩小握手面、保证每个 Wait 的生产者必达**，而不是先堆精度优化。

---

## 2. 工程不变量（暂行 · 反卡死）

下列不变量与具体算法步骤正交；违反任一条即回到「已知会挂」的设计区。

### I1 · 跨核面要短，能拆 launch 就拆

- **Host 边界 = 天然全局同步**。  
- 无 CrossCore 的 prep（纯 AIV）与有 CrossCore 的 MIX **不要塞进同一 kernel**，若业务允许两次 launch。  
- 重建路径：**Launch1 = AIV-only（H/G 等）→ mid-sync → Launch2 = MIX Encrypt**。

### I2 · CrossCore flag 表要极小且避开禁号

- 只用硬件 `CrossCoreSet/Wait`；**禁 SoftSync**。  
- **禁 flag 5、7**（工具链/硬件冲突）。  
- 重建路径：**仅 1/3 复用 + 4=GATE**。  
- stable Encrypt/Encaps 的 `l18_l19` 另用 **flag 2** 与 **GATE=8** —— 属更深 FSM，不作为安全模板。

### I3 · AIC Wait 环内禁止阻塞式本核同步

- Wait 等待对端时，**禁止**再 `SyncAll` / `IBWait` 等把本核堵死（见 cannbot sync-audit）。  
- KeyGen 里 Encode 后的 `SyncAll` 是 **AIV-only 汇合**，不在 AIC CrossCore Wait 环内 —— 与本条不冲突。

### I4 · 多 AIV 拆写共享矩阵时，生产者必须「全量可达」

- 若 Â（或同类全局表）按 AIV 对半写，而下游握手假定「表已满」，易出现 **半边 Â → 永远到不了 Set**。  
- 重建路径硬锁 **`F203_AHAT16_BLOCK_DIM=1`（及 CBD 同类）**：单逻辑写者写满。  
- stable KeyGen/Encaps prep 默认 **AHAT=2（8+8）** 在正确性路径上可用，但对 **Encrypt 式长 CrossCore 链** 放大了「未满表就握手」的风险面；重建刻意收窄。

### I5 · 禁把「曾绿的 stable 核」当同步模板

- CPU/SIM 绿 ≠ NPU 不挂。  
- fork `f203_encrypt_l18_l19` / alg14/20/21 树 = **继承同步债**（X5）。

### I6 · 验收以 NPU 返回为准，并用重复上板压 hang

- 单次绿不够；卡死目标要求 **同拓扑反复 `-r npu` 无 timeout**（例：T24×30）。  
- timeout 脏杀会污染同 `ASCEND_DEVICE_ID`（X3）——压测与排错须 Finalize/清会话。

---

## 3. 平台落地模型（AscendC）

### 3.1 推荐骨架（Encrypt/Encaps 类）

```text
Host:
  launch prep_custom   (AIV_ONLY, blockDim=1, 无 CrossCore)
  SynchronizeStream     ← mid-sync / TRACE 检查点
  launch compute_custom (MIX, blockDim=1；flag∈{1,3,4})
  SynchronizeStream

Launch2 内（示意）:
  AIV: Set(1) → … → Wait(3) → … → Set(4=GATE) → …
  AIC: Wait(1) → Cube… → Set(3) → Wait(4) → …
```

### 3.2 与「深融合 l18」的差异（概念）

| | 重建路径 | stable Encaps/Encrypt（l18_l19） |
|--|----------|----------------------------------|
| Launch2 职责 | Encrypt 链，flag 表短 | NTT+内积+GATE+INTT+pack **深 FSM** |
| GATE | flag **4** | flag **8**（另有 1/2/3/4） |
| Â 分核 | **强制 1** | 默认 prep **2** |
| 实机叙事 | T22–T24 不挂；×30 | SynchronizeStream **卡 l18** 史 |

表面都是「SIM/NPU 两次 launch」，**不能**据此认为拓扑等价。

---

## 4. 三路对照（原理层）

### 4.1 重建 Encaps（含 Encrypt）— 为何（暂行）不挂

同时满足 I1–I4：

1. **H/G 移出 MIX**：Launch1 无 Wait 对端，消除「头哈希卡住导致全程不 Set」。  
2. **握手面收窄到 1/3+4**：减少非法/易冲突 flag，缩短因果链。  
3. **Â/CBD 单写者**：降低半边数据导致的永久 Wait。  
4. **不以 stable l18 为模板**：避免把已知卡死 FSM 搬回来。

附加：liboqs 交叉/往返证明「跑完且语义对」——排除「假返回」。

### 4.2 stable Encaps / Encrypt — 为何容易挂

1. **同一 L2 深融合**：长 CrossCore 链上任一前缀失败 → 后续 Wait 全挂。  
2. **flag 2 + GATE 8**：FSM 状态更多，排障与误用面更大（重建明确不用这套）。  
3. **prep AHAT=2**：与长握手叠加时，半边/分工错误更致命。  
4. 工程侧已把 alg20/21 标为 **L18 高风险**，`probes_safe` **故意排除** Encaps/Decaps。

### 4.3 KeyGen — 为何历史上「少挂、多错」

1. **同样 2 launch**，但 L2 主要是 **NTT 段 1/2/3 + Encode/Tail**，**没有** Encrypt 式「内积→GATE→双路 INTT」同核长握手。  
2. 汇合用 **AIV-only SyncAll**（或 CPU 软旗），不落在 AIC Wait 环。  
3. 实机同批对照：**KeyGen=结果错；Encaps=卡 SynchronizeStream** —— 故障类型不同。  
4. 故：**不能**用「KeyGen 也能 AHAT=2」反推 Encrypt 长链也安全；KeyGen 的握手面本来就短。

### 4.4 一句话

| 路径 | 同步面 | 实机主风险 |
|------|--------|------------|
| 重建 Encaps | 短 flag + 双 launch 拆头 | 已压测：不挂（暂行） |
| stable Encaps/Encrypt | 深 l18 FSM | **卡死** |
| KeyGen | 短 NTT 握手 + AIV 汇合 | **算错**（相对少挂） |

---

## 5. 指导后续开发的检查单

新开或大改 **MIX 长链**（Encrypt / Encaps / 设备 Decaps / 任何「多段 CrossCore」）时，合入前自问：

| # | 检查 | 不通过则 |
|---|------|----------|
| 1 | 能否把无 CrossCore 的前缀拆成独立 launch？ | 必须论证为何 fused；并单独做 NPU hang 压测 |
| 2 | flag 是否 ⊆ {1,3,4} 或经 sync_audit 批准的等价小表？是否触及 5/7？ | 禁止上板 |
| 3 | AIC 任意 Wait 路径上是否调用 SyncAll/IBWait？ | 禁止 |
| 4 | 共享全局表（Â 等）是否存在「多 AIV 各写一半、下游假定已满」？ | 先改为单写者或显式汇合屏障 |
| 5 | 是否 fork/对照移植了 `l18_l19` / alg14/20/21 编排？ | 停；只读契约 |
| 6 | NPU 是否 **反复**跑通（建议 ≥10–30 次同命令）且无 timeout？ | 不得宣称「不挂」 |
| 8 | 写出 m/K/c 是否用 UB+DataCopy（禁依赖 GlobalTensor::SetValue 写 GM）？ | 见 X12；CPU 假绿常见 |

**Decaps / 新算子**：默认继承上表；若必须深融合，须先立 hang 假设与压测计划，再开精度。

### 5.1 Decrypt / Decaps 专项（2026-09-08）

用户实机卡死点是 **Encaps↔Decaps 来回**，不是单次 Encaps。

| 要求 | 说明 |
|------|------|
| T24 不足 | T24 用 **liboqs Decaps**，不能代替设备 Decrypt/Decaps |
| Decrypt | **禁止**采用 `F203-Alg15-…` 文中「生产 1-kernel + softSync + GATE 8」；须遵守本检查单 + NTT/INTT 分 launch 原理 |
| Decaps | 禁抄 alg21；复用重建 Encaps + 重建 Decrypt |
| 安心门禁 | 设备 Encaps→设备 Decaps 往返在 NPU **反复**跑通不挂（DAG `Q-RT-HANG`） |

---

## 6. 案例附录（路径）

### 6.1 重建（正例）

| 项 | 路径 |
|----|------|
| 设备 G Encaps | `graph-tests/enc_related/RB-T22-encaps-device-G/` |
| liboqs CROSS | `…/RB-T23-encaps-liboqs-cross/` |
| 往返 + ×30 压测 | `…/RB-T24-encaps-decaps-roundtrip/`；远程 log `encrypt-rebuild-hang-stress.log` |
| 运营约束 | `graph-tests/encrypt-rebuild-ops/COMMON.md` |
| KB | `docs/notes/Encrypt-cannbot-rebuild-kb.md` |

### 6.2 stable / pass-fix（对照 · 禁抄）

| 项 | 路径 |
|----|------|
| Encaps | `examples/stable/.../stable-fips203-mlkem-kem-encaps-k4/`；`pass-fix-f203-alg20-kem-encaps-device-k4/` |
| Encrypt | `…/stable-fips203-mlkem-pke-encrypt-k4/`；`pass-fix-f203-alg14-pke-encrypt-device-k4/` |
| l18 flag 8 | `…/compute/f203_encrypt_l18_l19_kernel.cpp`（`ST_AT_JP_GATE=8`） |
| 实机分流 | `docs/engineering/NPU真机环境说明.md` §4.4 |

### 6.3 KeyGen（对照）

| 项 | 路径 |
|----|------|
| KeyGen | `…/stable-fips203-mlkem-kem-keygen-k4/`；`pass-fix-f203-alg19-kem-keygen-device-k4/` |
| 笔记 | `docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md` |

### 6.4 对照结论来源

本总结综合了战役 KB、NPU 环境说明与 [对比调研](08ef480a-d7c0-46d5-a166-c391d8eb976e) 对三路径 STATUS/LAYOUT/flag 的核对。

---

## 7. 修订触发条件

出现任一情况 → **必须**改本笔记 + KB + DAG：

- 重建拓扑在干净卡上再现 SynchronizeStream 挂死；  
- 证明「flag 8 / AHAT=2 / 单 launch fused」在 Encrypt 长链上稳定不挂且可复现；  
- 设备 Decaps 采用不同同步面并形成新正/反例。
