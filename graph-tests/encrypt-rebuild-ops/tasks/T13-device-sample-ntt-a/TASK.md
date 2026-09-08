# T13 — 设备 Â←SampleNTT(ρ)（Alg.7 矩阵半链）

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T13` |
| 代码目录 | `graph-tests/enc_related/RB-T13-device-sample-ntt-a/` |
| 运营目录 | `…/tasks/T13-device-sample-ntt-a/` |
| 墙钟 | ≤ 80 min |

继承 COMMON。

## 目标

Host 预喂 **ρ[32]**（对齐 T07 ek 尾语义；**禁抄** encrypt/alg14）→ 设备 **MIX** 做：

\[
\hat A \leftarrow \mathrm{SampleNTT}(\rho \| i\| j)\quad (k{=}4)
\]

- 输出 Â 形状与 Encrypt 行 3–7 契约一致（对拍 golden）
- Flag：**1/3** 握手；**永禁 5/7**；可极轻 Cube×1
- 可复用活跃 Alg.7 / lines3-7 **I/O 契约与积木接口**；**禁止**大段抄探针实现、禁 frozen、禁 encrypt 树
- 主验收：**Â 对拍** + 不挂 + sync_audit

## 非目标

- CBD/y、NTT(y)、u/v 拓扑、pack→c、Encaps、设备 SHAKE 全自研（优先接已有积木）

## 必读

- COMMON · inventory 行 3–7 · KB §A · T07/T12 FEEDBACK
- cannbot CrossCore + sync-audit
- `pass-fix-f203-alg7-sample-ntt-k4` / `alg13-lines3-7-a-hat-k4` **契约**（勿大段抄码）

## 验收

- CPU + `SIM_DIRECT=1` sim；用例根无 stray
- sync_audit；中文注释；FEEDBACK PASS/FAIL
- 禁并行第二路 SIM；禁 SSH/NPU/改 KB/DAG

## 依赖

T07 ρ 语义；T12 证明设备半链可上板。
