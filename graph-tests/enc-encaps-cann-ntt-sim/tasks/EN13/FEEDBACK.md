# EN13 FEEDBACK — Encrypt × liboqs PKE 交叉（SIM-only）

| 项 | 值 |
|----|-----|
| 状态 | **PASS** |
| 日期 | 2026-09-11 |
| 目录 | `graph-tests/enc_cann_ntt/EN13-encrypt-liboqs-cross/` |
| 禁 | `-r npu`（未跑） |

## 命令与结果

```bash
cd graph-tests/enc_cann_ntt/EN13-encrypt-liboqs-cross
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | wall | tick | c vs liboqs |
|------|------|------|------|-------------|
| CPU | 0 | 1.831s | — | **max=0** |
| SIM | 0 | 132.858s | **812698** | **max=0** |

sync_audit 红线：**0**（仅 SYNC-09 性能级）。  
日志：`/opt/cursor/artifacts/enc-encaps-sim/en13-cpu.log`、`en13-sim.log`。

## 架构要点

- NTT/INTT：独立短 MIX（cann-ntt `mmad_custom`）；禁 GATE 4/8。  
- Âᵀ：Host 转置后喂既有 matvec。  
- ⟨t̂,ŷ⟩：新 AIV `enc_dot_real`。  
- **加噪 e1/e2/μ 在 Host**（设备做 SampleNTT/Prep/NTT/Matvec/Dot/INTT/Pack）。

## 教训

先过 Host `c≡liboqs`，再接线；EN09 的 INTT×512 与 FIPS/liboqs 不互逆，必须改为 ×3303。
