# T14 — 设备 Â + ŷ 半链拼装（SampleNTT∥NTT(y)）

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T14` |
| 代码目录 | `graph-tests/enc_related/RB-T14-device-ahat-yhat/` |
| 运营目录 | `…/tasks/T14-device-ahat-yhat/` |
| 墙钟 | ≤ 90 min |

继承 COMMON。

## 目标

单 launch **MIX**（或明确可证的双段同库）在设备侧产出：

\[
\hat A \leftarrow \mathrm{SampleNTT}(\rho),\quad
\hat y \leftarrow \mathrm{NTT}(y)
\]

- Host 预喂：ρ[32]、y[4×256]（或 coins→CBD→y，对齐 T07/T12；**禁抄** encrypt/alg14）
- 复用 T13/T12 **契约与积木接口**；禁止大段抄码、禁 frozen
- Flag：**1/3**；永禁 **5/7**；可极轻 Cube
- 主验收：**Â 与 ŷ 双对拍** + 不挂 + sync_audit

## 非目标

- MultiplyNTTs / INTT→u,v、pack→c、Encaps（留给后刀接 T10/T11）

## 必读

- COMMON · T12/T13 TASK+FEEDBACK · inventory 行 3–7 / 16
- cannbot CrossCore + sync-audit

## 验收

- CPU + `SIM_DIRECT=1` sim；用例根无 stray
- sync_audit；中文注释；FEEDBACK
- 禁并行第二路 SIM；禁 SSH/NPU/改 KB/DAG

## 依赖

T12、T13 本机绿（NPU 由主控补）。
