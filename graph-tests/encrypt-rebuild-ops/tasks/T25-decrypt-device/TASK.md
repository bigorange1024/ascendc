# T25 — 设备 Decrypt（Alg.15）反卡死重建

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T25` → `Q-RT-HANG` |
| 代码目录 | `graph-tests/enc_related/RB-T25-decrypt-device/` |
| 运营目录 | `graph-tests/encrypt-rebuild-ops/tasks/T25-decrypt-device/` |
| 墙钟 | ≤ 120 min CPU |

继承 [`COMMON.md`](../../COMMON.md)；**NPU 优先**；SIM skip 可交。

## 背景（必读）

用户实机卡死发生在 **Encaps↔Decaps 来回**。T24 仅用 **liboqs Decaps**，不能证明设备 Decrypt/Decaps 不挂。  
本刀起重建 **Decrypt**，再 Decaps，再设备往返压测。

反卡死：**[`MIX-Encrypt-Encaps-反卡死拓扑技术总结.md`](../../../../docs/notes/MIX-Encrypt-Encaps-反卡死拓扑技术总结.md) §5 检查单**。  
Alg.15 数学：笔记 `F203-Alg15-Decrypt-2launch编排技术总结.md` **只取数学与「NTT/INTT 勿同核」原理**；**禁止**采用文中「生产 1-kernel + softSync + GATE 8」路径（与本战役禁令冲突）。

## 目标

- 新目录实现 **Alg.15 Decrypt**：输入 `dk_pke`+`c`（+必要 LUT）→ 输出 `m[32]`。  
- 拓扑须满足：  
  - 能拆 launch 则拆（建议：prep AIV-only → mid-sync → NTT MIX → mid-sync → INTT+extract；或等价短 CrossCore 面）；  
  - flag **仅 1/3 + 可选 4=GATE**；**永禁 5/7、SoftSync、Wait 环 SyncAll**；  
  - `BLOCK_DIM=1`（及同类 AHAT/分核默认 1）；  
  - **禁抄** `pass-fix-f203-alg15*`、`stable|exp-*decrypt*`、`alg21*`、`l18_l19`、encrypt/encaps/decaps 树。  
- Golden：`m` 与权威一致（优先 **liboqs PKE Decrypt** 或仓内已验证 ref；缺库 **BLOCKED**，禁止假绿）。

## 验收

```bash
cd graph-tests/enc_related/RB-T25-decrypt-device
bash run.sh -r cpu -v Ascend910B4
# SIM skip 可；FEEDBACK 写明
```

- PASS：`m` 对拍 + sync_audit 无红线 + LAYOUT/STATUS 写清 launch/flag。  
- FEEDBACK → 主控立刻 NPU。

## 禁令提醒

COMMON 永禁第 1–2 条；检查单 §5 全勾。
