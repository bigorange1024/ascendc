# T15 — 设备 Encrypt 半链：Â+ŷ → u,v → c

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T15` |
| 代码目录 | `graph-tests/enc_related/RB-T15-encrypt-ahat-yhat-uv-c/` |
| 运营目录 | `…/tasks/T15-encrypt-ahat-yhat-uv-c/` |
| 墙钟 | ≤ 90 min（本机 CPU）；NPU 由主控上板 |

继承 COMMON；**战役 NPU 优先**：默认交 **CPU** 即可，长 SIM 非门禁。

## 目标

在设备侧贯通（可双 launch 同库，对齐 T11 外形）：

1. Â←SampleNTT(ρ)、ŷ←NTT(y)（接 T14 契约；或 Host 预喂其一作对照，但**最终路径须设备算**）
2. u,v ← MultiplyNTTs/INTT + 噪（接 T10）
3. pack → c[1568]（接 T06）

主验收：**c / u / v 对拍** + 不挂 + sync_audit（CPU）；主控立刻 NPU。

## 非目标

Encaps、KAT 全量、设备 CBD 全自研（可 Host 预喂 y/e/μ 若未就绪）。

## 必读

COMMON · T11/T14 FEEDBACK · inventory · cannbot CrossCore  
禁抄 encrypt/alg14/frozen；禁 flag 5/7。

## 验收

```bash
bash run.sh -r cpu -v Ascend910B4
# SIM 可选后补
```

FEEDBACK；STATUS；中文注释；禁 SSH/NPU/改 KB/DAG。
