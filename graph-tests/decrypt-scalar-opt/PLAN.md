# PKE Decrypt 融合核 · scalar / 跨核握手优化 · 全套 NPU 实验计划

> **状态**：计划已锁（2026-09-11）· **可上机**  
> **工作区**：`cursor/kem-2launch-sticky-1534`（禁止擅自开分支；commit/push 仅用户授权后）  
> **前置战役**：Launch 压缩 QUEUE 1–8 全绿；Decrypt Host launch 已 = **1**（`RB-D09-decrypt-1launch`）  
> **本战役目标**：在 **保持 Host launch=1、m≡liboqs、NPU×30 绿** 前提下，降低 Σ Task Duration；用 msopprof 证明 vector0 **scalar% 下降**或总 cycle 下降。  
> **空闲**：NPU 会话空转 ≤3 分钟——刀间立刻下一刀或停 keepalive 放机。

---

## 0. 已锁定的事实（不得当「再采一遍 Timeline」浪费板时）

| ID | 事实 | 证据 |
|----|------|------|
| F1 | `RB-D09-decrypt-1launch` Host **1 launch**，NPU×30 正确性已绿 | `graph-tests/dec_related/RB-D09-decrypt-1launch/STATUS.md` |
| F2 | Σ Task Duration ≈ **455–456 µs**（ops-profiling）；msopprof ≈ **454 µs** | `qa/active_npu_perf_summary.md` |
| F3 | Freq **1800/1800** 满频 | round_003 / STATUS |
| F4 | vector0 **scalar ≈ 96.8%**；cube 占比 ~0；大量 `scalar_wait_id*` / CrossCore 等待 | PipeUtilization / round_003 |
| F5 | 应用级 Chrome Trace / HTML **已有**；核内指令 Timeline **工具链不可得** | `docs/perf/round_003_pke_decrypt_1launch/README.md` |
| F6 | 设备流：`AIV0 Prep → SyncAll → (flag1/3)×2：Cube NTT↔AIV NTT+dot；Cube INTT↔AIV INTT+extract`；AIV1 仅握手 | `d09_decrypt_fused_custom.cpp` |

**推论（可推翻）**：性能刀应打在 **AIV0 标量热路径** 与/或 **AIC↔AIV 握手气泡**，不是再开 TimelineDetail、也不是再拆 Host launch。

---

## 1. 目标与非目标

### 1.1 目标（门禁）

| 级别 | 指标 | 通过线 |
|------|------|--------|
| **正确性** | `m[32]`≡liboqs PKE Decrypt；NPU×30 换 SEED | 必须 |
| **形态** | Host launch **=1**；`flag∈{1,3}`；sync_audit 无新增红线 | 必须 |
| **性能主指标** | Σ Task Duration（ops-profiling）相对 D09 基线 | **≤ 0.90× 基线**（≥10% 降）→ **有条件成功**；**≤ 0.80×** → **强成功** |
| **性能辅指标** | msopprof vector0 scalar% | 相对基线 **下降 ≥10 个百分点**为辅证；µs 降但 scalar% 不降须写清原因 |
| **回归（可选）** | 若积木可被 Decaps 复用：对 `RB-T30-decaps-2launch` NPU×5 冒烟 | 非默认必做 |

### 1.2 非目标

- 再压 Host launch（已是 1）。  
- 为「看起来忙」堆 Cube/MTE（与 F4 矛盾）。  
- 抄 stable / frozen / D08–D09 **源码**进新树（允许对照读 STATUS/注释）。  
- 用 CPU/SIM tick 或 host wall 填性能表。  
- 把「采不到指令时间轴」再当一刀实验。

---

## 2. 图谱推理（本战役专用）

机器可读图：[`docs/rg-decrypt-scalar-opt.yaml`](../../docs/rg-decrypt-scalar-opt.yaml)。

```text
F1..F6（已测）
    │
    ├─► J1: 主瓶颈 ∈ {握手气泡, AIV0 标量数学, 二者叠加}
    │         （cube 空等 ≠「该上更多 matmul」）
    │
    ├─► Q1: PrepUnpack / ByteDecode₁₂ 标量 GM 环占多少 wall？
    ├─► Q2: NTT/INTT 设备蝶形 for-loop 标量占多少？
    ├─► Q3: 每轮 CrossCore Set/Wait + 双侧 PipeBarrier 气泡占多少？
    ├─► Q4: AIV1 仅握手、AIV0 独算 — 双 AIV 拆多项式能否降 wall？
    │
    └─► 实验序：W0 钉基线 → W1 静态热点清单 → 按风险从低到高试 H1..H4
              每刀：sync_audit → NPU 单轮 → ×30 → profiling 对比 → 回写图
```

**刀前强制**：读本 PLAN §0 + 图谱 yaml + D09 STATUS；禁止跳过 W0 直接大改。

---

## 3. 假说矩阵（按风险从低到高）

| ID | 假说 | 预期机制 | 主要风险 | 新目录（建议） | 先决 |
|----|------|----------|----------|----------------|------|
| **H0** | 复测 D09，基线数字稳定 | 无代码变更 | 环境/频点漂 | （沿用 D09） | — |
| **H1** | CrossSet/Wait 外包的 `PipeBarrier<PIPE_ALL>` 过重，可收窄到必要 pipe | 减握手气泡 | 同步欠约束→挂/错数 | `RB-D10a-handshake-thin` | H0 |
| **H2** | Prep / `poly_byte_decode12_scalar_gm` 与解包环是 AIV0 scalar 大头 | 块搬 + 向量解码 | 解码错→m 错；对齐 | `RB-D10b-prep-vec` | H0；W1 指向 prep |
| **H3** | `ntt_device_math` / `intt_device_math` 标量蝶形环是大头 | 向量蝶形或迁已验证 NTT 积木（禁抄旧 fused 树） | 正确性；UB | `RB-D10c-ntt-vec` | H0；W1 指向 ntt |
| **H4** | AIV1 空转可分担 poly 维（k=4 拆分）缩短 AIV0 临界区 | 降 wall；同步更复杂 | 双 AIV 竞争；flag 生命周期 | `RB-D10d-dual-aiv` | 前刀绿且收益不足 |
| **H5**（禁优先） | 再拆回多 Host launch「好 profile」 | — | 违背战役目标 | — | **禁止作主路径** |

**决策规则**

1. W1 必须给每个热点「高/中/低」占比档（循环次数×访存粗估即可）。  
2. **先打「高」且风险更低的假说**；编码可并行准备，**NPU 上板一次一刀**。  
3. 正确性绿但 µs 改善 &lt;3% → **证伪该假说为主因**，换刀；禁止同假说磨板。  
4. 挂死/错数 → 停、写 FEEDBACK、**不硬融**；不改 D09。

---

## 4. 波次日程（上板顺序）

### Wave 0 — 基线钉死（H0）· ≤30 min

| 步 | 动作 | 产出 |
|----|------|------|
| 0.1 | 确认 Freq 1800/1800；`ASCEND_DEVICE_ID` 与物理卡一致 | 日志一行 |
| 0.2 | `cd graph-tests/dec_related/RB-D09-decrypt-1launch && bash run.sh -r npu -v Ascend910B4` | PASS |
| 0.3 | NPU×5 冒烟（换 SEED） | ok=5 |
| 0.4 | ops-profiling Σ + msopprof PipeUtilization（同 round_003 口径） | `tasks/DS-W0/FEEDBACK.md`；偏差 &gt;5% **先查环境** |

**失败**：不进入 Wave 1+。

### Wave 1 — 静态热点清单（可离线）

| 步 | 动作 | 产出 |
|----|------|------|
| 1.1 | 通读 `d09_decrypt_fused_custom.cpp`、`prep_*`、`ntt_*`、`intt_*` | — |
| 1.2 | 统计 CrossSet/Wait 次数、每侧 PipeBarrier、prep/NTT/INTT 标量环 | `tasks/DS-W1/HOTSPOTS.md` |
| 1.3 | 给 H1–H4 排出唯一推荐上板序 | 写入 [`QUEUE.md`](QUEUE.md) |
| 1.4 | 回写图谱 Q1–Q4 倾向 | `docs/rg-decrypt-scalar-opt.yaml` |

### Wave 2+ — 按 QUEUE 推荐序逐刀

对每一刀 `DS-Hx`：

1. **新建** `graph-tests/dec_related/RB-D10*`（独立新树；禁改 D09）。  
2. **只改本假说相关面**；I/O / MAGIC / launch=1 契约不变。  
3. sync_audit：
   ```bash
   python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
     --check all --format json <RB-D10*目录> \
     > graph-tests/decrypt-scalar-opt/tasks/DS-Hx/logs/sync_audit.json
   ```
4. NPU 单轮 → 绿则 **×30**。  
5. 同 W0 口径 profiling；填 FEEDBACK 对比表。  
6. 更新 QUEUE + 图谱节点 status。

### Wave 终 — 收口

| 条件 | 动作 |
|------|------|
| ≥1 刀达到有条件/强成功 | 刷新 `qa/active_npu_perf_summary.md`；写当日 qa 纪要；更新 `AGENT_HANDOFF.md` |
| 全证伪且无 ≥3% 收益 | 写「性能墙=当前算法结构」判决；停磨 D09；是否换路线交用户 |
| 板空闲 | 停 keepalive |

---

## 5. 上机命令清单（复制即用）

```bash
export ASCEND_DEVICE_ID=0   # 按 which_npu / 实测；勿写死错卡
# 确认满频后再采数

cd graph-tests/dec_related/RB-D09-decrypt-1launch   # 或 RB-D10*
bash run.sh -r npu -v Ascend910B4
# ×30：按该目录既有脚本 / STATUS 口径换 SEED

python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  --check all --format json "$PWD"

# Σ Task Duration：走 cannbot ops-profiling（与六档表同口径；具体入口以 skills 文档为准）
# cycle / scalar%：msopprof PipeUtilization（同 round_003）
```

> 新树 kernel 名随目录替换；**禁止**并行多路 `run.sh -r npu` / 多路 msopprof。

---

## 6. FEEDBACK 最低字段（每刀）

```markdown
| 项 | 值 |
|----|-----|
| 假说 | Hx |
| 目录 | RB-D10* |
| sync_audit | 绿/红（路径） |
| NPU×30 | ok/fail |
| Σ µs（基线→本刀） | |
| vector0 scalar%（基线→本刀） | |
| 判决 | 采纳 / 证伪 / 有条件 |
| 图谱回写 | 已/未 |
```

---

## 7. 与旧图 / 旧战役关系

| 资产 | 关系 |
|------|------|
| `docs/rg-decrypt-fused.yaml`、hang 图 | **只读**；管卡死，不替代本性能图 |
| `docs/rg-decrypt-scalar-opt.yaml` | **本战役**假说/证伪落点 |
| `graph-tests/launch-reduce-ops/` | Launch 数战役 **已收口**；本战役是其 **性能后续** |

---

## 8. 成功画像

在 **仍是 1 次 Host launch、×30 对 liboqs 绿** 的前提下，Decrypt 融合核 Σ Task Duration 相对 D09 基线下降 ≥10%，并用 msopprof 说明 scalar/握手气泡被打薄——而不是又交一份「采不到指令时间轴」的报告。
