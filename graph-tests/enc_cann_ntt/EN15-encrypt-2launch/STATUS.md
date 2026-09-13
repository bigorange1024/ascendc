# EN15-encrypt-2launch · STATUS

> 日期：2026-09-13（I/O 整改）
> 结论：**整改中 / SIM 待验** — 生产 I/O 仅 `ek_pke|m|coins` + LUT；设备 CBD→y/e1/e2，μ←m；禁中间态 .bin 读入

## 外形

| Launch | 核 | 内容 |
|--------|-----|------|
| 开场 H2D | — | ek 解码ρ/t̂（Host 内存）+ coins + m + LUT；一次就位 |
| L1 | `enc_prep_l1` | SampleNTT(ρ)→Â + CBD→y/e1/e2（留设备） |
| L2 | `enc_compute_l2` | NTT→matvec(Âᵀ下标)→…→μ←m→pack→c |

## 门禁

- Host launch = 2
- 禁止 `input/{rho,sigma,t_hat,e1,e2,mu}.bin` 等中间态
- `c` vs liboqs max=0
