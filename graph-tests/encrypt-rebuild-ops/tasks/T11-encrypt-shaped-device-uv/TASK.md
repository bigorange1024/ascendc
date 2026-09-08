# T11 — Encrypt 外形：设备 u,v（接 T10）+ pack

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T11` → 逼近 Q-ULT |
| 代码目录 | `graph-tests/enc_related/RB-T11-encrypt-shaped-device-uv/` |
| 运营目录 | `…/tasks/T11-encrypt-shaped-device-uv/` |
| 墙钟 | ≤ 75 min |

继承 COMMON。

## 目标

在 **新目录** 拼装（禁抄 alg14/encrypt/encaps/decaps/frozen）：

1. Launch1：prep（T07/T09 语义：ρ/coins→y,e1,e2 半桩可）
2. Launch2：MIX = T03 握手纪律 + **T10 级设备算 u,v**（Host 可预喂 Â/ŷ/t̂/e/μ；**禁** Host 孪生 u,v 当唯一路径）+ T06 pack→c[1568]

主验收：CPU+SIM **PASS_SYNC**（不挂）；**PASS_IO** 尽力（c 与 golden）；分栏写清。

## 非目标

- 设备 SampleNTT(ρ) / 真 NTT(y) 全链（可后刀）
- Encaps 外壳 / KAT

## 必读

T09/T10 FEEDBACK · KB §A2 · X1/X6/X9 · COMMON

## 验收

- sync_audit；FEEDBACK；中文注释
- 禁并行第二路 SIM；禁 SSH/NPU
- 主控会在用户开机后并行上板——你只本机

## 依赖

T09 + T10 = PASS（本机）。
