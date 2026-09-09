# DRW-K04 — 设备 Encaps ↔ 设备 Decaps 往返 + 反卡死压测

| 字段 | 值 |
|------|-----|
| 状态 | **PASS_CPU**（待主控 NPU） |
| DAG | `E-K04-RT` → `G-DG7-RT` / **`Q-RT-HANG`** |
| 代码目录 | `graph-tests/dec_related/RB-D07-enc-decaps-rt/`（**新建**） |
| 运营目录 | `graph-tests/decrypt-rebuild-ops/tasks/DRW-K04-enc-decaps-rt/` |
| 墙钟 | ≤ 180 min（CPU 拼装）；NPU ×30 由**主控** |
| runner | **subagent**：拼装 + **CPU only**；**禁止** `-r npu` / ×N / SSH |

继承 [`COMMON.md`](../../COMMON.md)。前置：**Q-DECAPS-CORRECT 已关**。

## 目标

```text
设备 Encaps（契约接 T22–T24 思路，禁抄源码）
  → (c, K_enc)
设备 Decaps（本战役链：Decrypt 三 launch + G + ReEnc + FO；可多 launch + mid-sync）
  → K_dec
验收：K_dec ≡ K_enc ≡ liboqs Encaps/Decaps（同 dk/ek/m）
```

- 优先：**编排已绿积木**（复制/引用 `RB-D04-decrypt-full`、`RB-D04-decaps-G`、`RB-D05-decaps-reenc`、`RB-D06-decaps-fo` 与 Encaps 侧活跃积木契约）进单用例 Host 多 launch；basename **全局唯一**（禁再引入裸 `prep_custom`）。  
- Encaps 侧：可新建 `encap_*` 核或从 **活跃** encrypt-rebuild 积木 **契约重拼**（禁 fork `RB-T25…T27` / stable Encaps 深 FSM）。  
- Host：单 session 多 launch + 每次 mid-sync；**禁**两段 `aclFinalize` 当基线（X16）。  
- X12；flag∈{1,3,4}；`BLOCK_DIM=1`。  
- sync_audit 必跑。

## 非目标（Subagent）

- **不做** NPU ×30（主控专属）。  
- 不重开已关 Decrypt/FO 算法语义（修 bug 须 FEEDBACK 说明）。

## 必读

1. S0B FEEDBACK §1 K04 行、§5 压测、§6 basename 检查单  
2. Encrypt KB §B2 + T22–T24 FEEDBACK **思路**（禁抄源码）  
3. Decrypt KB · X16 · COMMON · 反卡死 note §5  
4. 本战役已绿目录 STATUS（可复制进 RB-D07）

## 禁令

- 禁抄 / fork T25–T27、alg15/21、examples decrypt|decaps|encaps、frozen、`l18`。  
- 禁子进程 liboqs 当生产 Decaps。  
- 禁改 Decrypt KB/DAG。

## 验收（Subagent）

```bash
cd graph-tests/dec_related/RB-D07-enc-decaps-rt
bash run.sh -r cpu -v Ascend910B4
# sync_audit → 本刀 logs/
```

| 项 | 判据 |
|----|------|
| 往返 | `K_dec ≡ K_enc ≡ liboqs` max=0 |
| 形态 | 多 launch + mid-sync；basename 无撞 |
| sync_audit | 无红线 |

## 回报

FEEDBACK；`next_hint`：**请主控 `-r npu` ×1 再 ×30（预算 180）关 Q-RT-HANG**。

## 主控后续

1. NPU ×1 绿  
2. 同命令 ×30：`ok=30 fail=0`；超时 124=缺陷  
3. 关 `Q-RT-HANG`；战役收口；**停心跳放机**
