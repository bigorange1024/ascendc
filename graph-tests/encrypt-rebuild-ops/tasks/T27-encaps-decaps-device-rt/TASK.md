# T27 — 设备 Encaps→Decaps 往返 + NPU 不挂压测

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T27` → `Q-RT-HANG` |
| 代码目录 | `graph-tests/enc_related/RB-T27-encaps-decaps-device-rt/` |
| 运营目录 | `graph-tests/encrypt-rebuild-ops/tasks/T27-encaps-decaps-device-rt/` |
| 墙钟 | ≤ 120 min CPU |

继承 COMMON；**NPU 优先**。

## 背景

用户实机卡死点是 **Encaps↔Decaps 来回**。T24 用 liboqs Decaps 不足；本刀必须：
- **设备 Encaps**（T22/T23 路径）出 `(c,K)`
- **设备 Decaps**（T26）出 `K'`
- `K'≡K`；且 NPU **反复**跑不挂

必读：反卡死总结 §5/§5.1；X12（DataCopy 写 GM）。

## 目标

新建目录，编排/复用自研树（`RB-T23` Encaps + `RB-T26` Decaps），**禁抄** alg20/21/stable。
- Host：liboqs KeyGen → ek/dk；固定或随机 m；设备 Encaps → 设备 Decaps
- 验收：`K_decaps ≡ K_encaps`（并可与 liboqs 交叉，缺库 BLOCKED）
- `BLOCK_DIM=1`；flag 规则同 T23/T26；写出路径用 DataCopy

## 验收

```bash
cd graph-tests/enc_related/RB-T27-encaps-decaps-device-rt
bash run.sh -r cpu -v Ascend910B4
```

FEEDBACK 交后主控 NPU；主控另跑 ×N 压测关 `Q-RT-HANG`。
