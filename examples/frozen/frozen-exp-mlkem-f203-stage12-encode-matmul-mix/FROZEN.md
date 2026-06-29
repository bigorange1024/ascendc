# ⛔ 已冻结（2026-06-11）

**原路径**：`examples/incubating/exp-mlkem-f203-stage12-encode-matmul-mix/`

## 冻结原因

| 类别 | 说明 |
|------|------|
| **路线错误** | CANN LeakyRelu **融合 MIX** + `Matmul<>` 企图单 kernel 覆盖 Stage1 encode + Stage2 |
| **跑不通** | SIM encode 写 GM 失败；CPU 需两趟 launch 绕过 CrossCore |
| **不可维护** | AIV→AIC→AIV 边界被模板封装，workspace/Iterate/CrossCore 不透明 |
| **customspec 失效** | 目录内 `*-customspec.pdf` **禁止**再作新 exp 实现依据 |

## 继任（勿抄本目录）

- NTT 工程壳：`merged_kyber` FSM + `AicMmad` — 见活跃 `examples/incubating/exp-sepolyvec8-ntt-k8/`
- F203 Tag5T 集成：`ascendc-tests/frozen/frozen-fix-f203-2s1e-alg13-16171820-k4`

**禁止** fork 本目录、禁止抄 MIX 三段壳、禁止 NTT 内 `Matmul<>` 融合路线。
