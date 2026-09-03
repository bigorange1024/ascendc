# Decaps K≈131 — 排查计划（从图谱导出）

> 主控维护。图谱：`docs/rg-kem-decrypt-k131.yaml` · Viewer：`docs/rg-kem-decrypt-k131.html`  
> 与 Encaps 粘性正交；**同时仅一个 subagent**；迭代门禁 = **SIM**。  
> 最终：NPU 上 `K.bin` 与 accept golden `max_abs_diff=0`。

---

## 1. 已沉淀事实

| 节点 | 含义 |
|------|------|
| `F-npu-k-max-131` | 实机默认 2-launch Decaps 恒红，K max≈131（整段量级，非噪声） |
| `F-cloud-sim-2launch-pass` | 同日 Cloud 曾 SIM 绿（须本工作区复验） |
| qa §6 假绿三问 | golden 同源？日志是否 `chain_ntt`/`prep_ntt`？正交 A/B/C |

## 2. 优先假说（可证伪）

| 假说 | 预期证据 | 证伪条件 |
|------|----------|----------|
| **H1 FO 隐式拒绝** | `output/K` ≈ `J(z‖c)`，≠ accept `K` | K 既不等于 J 也不等于 accept，或等于 accept 但 cmp 脚本误读 |
| **H2 Phase-D `m'` 错** | dump `m'` ≠ encaps 所用 `m`；A 路径（D fused）绿 | A 仍红且 m' 正确 |
| **H3 Phase-E 重加密/`c'` 错** | `c'≠c` 触发拒绝；B 路径（E split）绿 | B 仍红且 c'==c |
| **H4 仅 NPU 同步/可见性** | 本仓默认 SIM 仍绿；A/B/C 在 NPU 分叉 | SIM 已红 → 先修 Cloud 回归 |

## 3. 实验阶梯（勿跳）

### Step-0（Cloud，下一刀）

```bash
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4
KEM_DECAPS_FORCE_REBUILD=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

日志必须出现 `chain_ntt` / `prep_ntt`。记录 K max_abs_diff。

- **若红** → Step-1 分段（同 SIM：A/B/C env）定位 D vs E。  
- **若绿** → 沉淀 `F-sim-still-pass`；转 Step-2（用户 NPU 正交）+ 可选中间量 dump（仅调试 env）。

### Step-1（SIM 分段，仅 Step-0 红时）

| 代号 | env | 含义 |
|------|-----|------|
| A | `F203_DECRYPT_FUSED=1` | D 回 fused；E 默认 |
| B | `F203_DECAPS_SPLIT_PREP=1` | D 默认；E 回 3-launch |
| C | `F203_DECRYPT_FUSED=1 F203_DECAPS_FUSED_L18=1` | D+E 都回 main 融合 |

### Step-2（用户 NPU，SIM 绿之后）

同 qa §6 A/B/C；外加诊断（若可加调试开关）：

1. 比 `output/K` vs Host 算的 `J(z‖c)`（确认 H1）  
2. 可选 dump `m'` / `c'`（仅调试；勿当生产默认）

### Step-3（修法）

按定位段修；禁止 correctness 反模式（碎写 GM / 滥 launch）。若与 Encaps Host 折 μ 共享 `l18` 内核，改动须两边图谱联动。

## 4. 与 Encaps 线的边界

| 可共享 | 不可混 |
|--------|--------|
| SoftSync/GATE 定式、SET(4) 可达性教训 | 把 Encaps 粘性热修当 Decaps K 的充分解 |
| `acl_session` / FORCE 纪律 | 并行 SIM；同 TASK 改两条线 |

## 5. 非目标（本阶段）

- 不新建 Decaps stable；不晋级 incubating  
- 不以 CPU PASS 结案  
- 不把「max=131」当噪声容忍  
