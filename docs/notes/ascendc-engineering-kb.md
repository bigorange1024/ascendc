# AscendC 工程知识库（短 KB）

> **目标经验包**：写出 **正确且不卡死** 的 ML-KEM AscendC 算子（反卡死是优先风险，**不是**唯一内容）。  
> **知识**＝经得起反复检验的经验；实验冲突须刷新本文件与图谱。  
> **不是知识**＝某次战役的 launch 拼法日记、刀号 PASS（→ QUEUE / HANDOFF）。  
> **图谱**：[`docs/rg-ascendc-engineering.yaml`](../rg-ascendc-engineering.yaml) · `python3 scripts/rg_viz.py`  
> **长文展开**：NTT 契约 · MIX 反卡死总结 · DataCopy/TQue 等 notes  
> **刷新**：2026-09-09（六算子 stable↔重建 launch 对照；KeyGen NPU×30 关闸；设备字符串 `__gm__`）

---

## 0. 知识库三条作用（章程）

| # | 作用 | 本仓落地 |
|---|------|----------|
| 1 | 把编程会用到的经验总结进来，帮助写码 | 卡死法则 **与** KEM 数学/数据/平台写码事实同收 |
| 2 | 实验中有价值推理入库；与结果冲突则改旧知识 | 图谱 `retracted`/`inactive` + 本文件同步改写 |
| 3 | 形成可指导后续开发的经验包 | 换 Agent 只靠 KB+图，应能推理「怎样写才对且不挂」——用法案例仍看长文 |

**优先级**：先保证不挂的握手与拓扑空间，再在该空间内满足 liboqs 正确性；两者都是交付门禁，缺一不可。

**入库**：可再测、与刀号无关。  
**不入库**：「今日 L1→L2a→L2b」「flag 选了 {1,3,4}」这类**用法流水**（约束事实可进；拼法日记不进）。

---

## 1. 双门禁（正确 ∧ 不卡）

1. **卡死 ≠ 算错** — 证明其一不蕴含另一；交付须两门都过。  
2. **权威正确性** — I/O 与同 ISA 的 liboqs（或登记权威）字节等价；golden 只做 oracle，不要求与参考源码同构。  
3. **不卡死** — 干净卡 NPU 上同拓扑反复加压无 `SynchronizeStream` 超时；CPU/SIM 绿不能结案「不挂」。  
4. **假绿** — `SetValue`→GM、异架构 ref、半写可见性未屏障，均可「看似绿、实机错/挂」。

---

## 2. 反卡死（优先风险类）

### 2.1 法则

- 跨核挂 ≈ 已 `Wait(f)` 而对端本次执行中 **不可达** `Set(f)`（含 Wait 环内再阻塞）。  
- 共享表若被假定「已满」却半写且无全局可见屏障 → 可能永不 `Set` 或读半成品。  
- 同组硬件 CrossCore flag 被两段 MIX 争用 → 错数或互锁。  
- 结构缺 `SET`：SIM 可构造；**粘性/污染挂**须干净 NPU；timeout 后同卡连环挂是次生污染。

### 2.2 平台事实（反复检验）

| 事实 |
|------|
| flag **5/7** → 超时/挂（永禁） |
| AIC 在 CrossCore `Wait` 中 `SyncAll`/`IBWait` → 互锁（永禁） |
| 自造 AIC↔AIV SoftSync → 挂（永禁） |
| 省略与 `Wait(4)` 配对的 `SET(4)` → SIM 可 124 |
| basename 撞名 → auto_gen 吞核 / 507000 |
| 旧 fused 二进制未 FORCE → 工程粘性 |
| `__aicore__` 字符串字面量是 `__gm__ char[]`，不可赋 `const char*`（CPU 可过、NPU 编不过）→ `constexpr uint8_t[]` |

### 2.3 已撤回假说

- 「同核双 Cube ⇒ 充分挂因」**retracted**  
- 「Host launch 计数加多 ⇒ 充分修复」**retracted**（若拆 launch 有效，有效的是消除断裂/争用，不是「次数本身」）

---

## 3. 正确 KEM 写码经验（与反卡死并列入库）

下列来自本仓已定稿契约与探针，**换参数须保持结构不变**；细节与公式见长文。

### 3.1 NTT / 并行语义

| 经验 | 要点 |
|------|------|
| Tag5T 三段式 | Encode(limb) → MMAD×2 → RouteA 重组 + mod \(q\)；输出对齐 FIPS/`MlkemNtt` 语义 |
| **poly-batch** | Stage2 后每个 AIV 握有完整 poly 的 hi+lo；**禁** limbsplit（hi/lo 分核） |
| Stage1 切分 | 按 poly 批切整行；禁按系数半维切到不同 AIV |
| limb 低位 | \(\mathrm{lo}=v-(v\gg 6)\cdot 64\)，**不是** `v&63` |
| Gather 范围 | **仅** NTT S1–S3 内禁 Gather；其后 basemul/ByteEncode 等另议 |
| 活跃 2s1e 数据面 | 平面 `mat_c`；host 逻辑 1s+1e，设备内复制 \(\hat{s}\)；禁 se_pair GM 交换路线 |

### 3.2 正确性与实现对拍

| 经验 | 要点 |
|------|------|
| golden = I/O oracle | 设备可用 Barrett/向量路径，最终多项式/字节一致即可 |
| 业务 GM 写出 | UB + DataCopy；禁依赖 `GlobalTensor::SetValue`（CPU 假绿） |
| 交叉同 ISA | 禁 x86 `liboqs_*_ref` 覆盖 aarch64 真机 |
| 半成品 | prep 写出未全局可见就开 NTT → û 等半成品（既是错数源，也可通向挂） |
| 设备侧字符串 | 见 §2.2；前缀常量用 `constexpr uint8_t[]` 拷 UB |

### 3.3 与「不挂」的交界

- 长 CrossCore / 深 GATE FSM / 多写者半表：放大「到不了 Set」面（反卡死）**且**放大半成品错数面（正确性）。  
- 收窄握手面、保证生产者全量可达，是 **正确∧不卡** 的共同地基。

### 3.4 stable ↔ 重建：六算子 Host launch 数（SIM/NPU 生产口径）

> 图谱节点：`F-REBUILD-VS-STABLE-LAUNCH`。  
> **不含** CPU 孪生分叉（旧 Encrypt/Encaps CPU 常 5、Decaps CPU 常 6）。  
> **次数相同 ≠ 拓扑同构**（重建多为 `blockDim=1`、短 MIX、Host mid-sync）。

| 算子 | FIPS | stable | 重建（graph-tests） |
|------|------|--------|---------------------|
| PKE KeyGen | Alg.13 | **2** | **3**（`RB-K04`） |
| PKE Encrypt | Alg.14 | **2** | **2**（`RB-T19`） |
| PKE Decrypt | Alg.15 | **1** | **3**（`RB-D04`） |
| KEM KeyGen | Alg.19 | **2** | **4**（`RB-K06`） |
| KEM Encaps | Alg.20 | **2** | **2**（`RB-T22`） |
| KEM Decaps | Alg.21 | **3** | **5**（`RB-T26`） |

**重建基线口诀**：Encrypt/Encaps=2；Decrypt=3；PKE-KG=3；KEM-KG=4；Decaps=5。

**落点**：stable 在 `examples/stable/...`（交付默认）；重建在 `graph-tests/{kg,enc,dec}_related/`（incubating；晋级须 `#交付#`）。  
KeyGen 重建门禁（正确∧不挂）已关：`Q-KEYGEN-CORRECT` / `Q-KEYGEN-HANG` → answered (`answered_by=F-REBUILD-KEYGEN-NO-HANG`)；证据 `F-REBUILD-KEYGEN-NO-HANG`。

---

## 4. 图谱与刷新

| 内容 | 落点 |
|------|------|
| 可机读断言（含证伪） | `rg-ascendc-engineering.yaml` |
| 人读摘要 | **本文件**（与图冲突时以实验刷新两者） |
| 用法案例 / 战役进度 | 长文附录 · QUEUE · HANDOFF |

校验 / 可视化：

```bash
python3 scripts/check_rg_dag.py --yaml docs/rg-ascendc-engineering.yaml
python3 scripts/rg_viz.py
```
