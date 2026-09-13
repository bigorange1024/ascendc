# EP06-encaps-2launch · STATUS

> 日期：2026-09-13（I/O 整改）
> 结论：**整改中 / SIM 待验** — 输入仅 `ek_kem|m` + LUT；Host FO 内存派生 r（不落盘）；Encrypt 同 EN15 设备噪声/μ

## 外形

| 阶段 | 内容 |
|------|------|
| Host FO | H(ek)/G(m‖H)→K（写 output）与 r（仅内存→H2D） |
| L1/L2 | 同 EN15：设备 CBD y/e1/e2，μ←m；无 mid 业务 H2D |

## 门禁

- Host launch = 2（FO 不计）
- 禁止预填 `sigma/coins/e1/e2/mu/rho/t_hat` 作为工程输入
- `c`/`K` vs liboqs max=0
