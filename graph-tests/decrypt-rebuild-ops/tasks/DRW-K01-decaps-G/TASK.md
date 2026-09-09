# DRW-K01 — Decaps 设备 G(m'‖h) → (K'‖r')

| 字段 | 值 |
|------|-----|
| 状态 | **dispatched** |
| DAG | `E-K01-G` → `G-DG4-G` |
| 代码目录 | `graph-tests/dec_related/RB-D04-decaps-G/`（**新建**；与 `RB-D04-decrypt-full` 并存） |
| 运营目录 | `graph-tests/decrypt-rebuild-ops/tasks/DRW-K01-decaps-G/` |
| 墙钟 | ≤ 60 min |
| runner | **subagent**：编码 + **CPU only**；**禁止** `-r npu` / SSH |

继承 [`COMMON.md`](../../COMMON.md)。前置：`Q-DEC-CORRECT` **已关**（D04 NPU+liboqs）。

## 目标

单 **AIV-only** launch（无 CrossCore / 无 MIX）：

```text
输入：m'[32] ‖ h[32]（h 自 dk_kem[3104:3136) 切片，禁默认可重算 H(ek) 冒充）
设备：SHA3-512 G → 64B → 拆 K'[32] ‖ r'/coins[32]
写出：UB + DataCopy → GM（X12）；Host mid-sync 后 D2H
```

- basename 建议：`decaps_g_custom.cpp`（全局唯一；勿与 `dec_*` / Encaps `prep_custom` 撞名）。  
- Host **不**预喂最终 `K'`/`r'`；golden 仅作对拍。  
- 权威：优先 `liboqs` / host 同输入的 `G` 字节；缺 aarch64 库时 host SHA3-512 oracle 可过 CPU，FEEDBACK **须标明**。

## 非目标

- 不做 Re-Encrypt / FO / 往返。  
- 不把 Decrypt 三核并进本 binary。  
- 不跑 NPU（主控后续）。  
- **禁止** fork / 对照源码移植 `RB-T22` / T25–T27 / alg21 / examples decaps。

## 必读

1. S0B FEEDBACK §1 K01 行 + §3 GM 契约（`m'/h/K'/r'`）  
2. Decrypt KB §A2 / inventory DG4  
3. T22 FEEDBACK **思路 only**（设备 G、Host 不预喂 coins）— **禁抄** `RB-T22` `.cpp/.hpp`  
4. `library/shared` SHA3（若有）契约；cannbot sync_audit  
5. COMMON

## 禁令

- 禁抄 T22–T27 / Encaps / Decaps / frozen 实现源码。  
- 禁用 `H(ek)` 设备重算替代 **dk 切片 h**（本刀锁定切片语义）。  
- 禁 `GlobalTensor::SetValue` 写 `K'`/`r'`。  
- 禁改 Decrypt KB/DAG。

## 验收（Subagent）

```bash
cd graph-tests/dec_related/RB-D04-decaps-G
bash run.sh -r cpu -v Ascend910B4
# sync_audit → 本刀 logs/
```

| 项 | 判据 |
|----|------|
| CPU | `K'`、`r'` 各 32B ≡ oracle max=0 |
| 形态 | 单 AIV launch；无 flag 5/7 |
| sync_audit | 无红线（假阳性写入 FEEDBACK） |
| INDEX | 更新 `graph-tests/dec_related/INDEX.md` |

## 回报

FEEDBACK 头块 + STATUS；`next_hint`：**请主控开机后推 `-r npu`**。

## 主控后续

`which_npu` + 侧树 rsync + aarch64 liboqs（若需）→ `-r npu -v Ascend910B3` → 绿则派 K02。
