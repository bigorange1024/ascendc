# 2026-09-11 · Encrypt+Encaps × cann-ntt · SIM-only 战役计划落盘

- **入口**：`graph-tests/enc-encaps-cann-ntt-sim/`（INDEX / PLAN / QUEUE）
- **图谱**：`docs/rg-enc-encaps-cann-ntt-sim.yaml`（`rg_validate` OK；HTML → `/opt/cursor/artifacts/rg-enc-encaps-cann-ntt-sim.html`）
- **锁定**：Host 多段 + 迁入 cann-ntt MIX NTT/INTT；ML-KEM-1024；主目标不挂 + 本阶段补 SIM 正确性与 Encaps
- **运行**：仅 CPU + `SIM_DIRECT=1`；**禁 NPU / 禁 keepalive**（用户休息勿空转）
- **继承**：`enc_cann_ntt` EN01–EN12 不挂已齐；不重做同质不挂刀
- **SIM 序**：W0b 基线复验 → EN13 liboqs 交叉 → EN14 sticky → EP01–EP05 Encaps → NPU 刀封锁至用户授权
- **cannbot**：tiling-design / api-best-practices / sync-audit / precision-debug（W1+）；禁本阶段 ops-profiling 上板
- **下一刀**：EE-W0b（复跑 EN09 或 EN07 cpu+SIM）

---

## EN13 落地（同日追加）

- **目录**：`graph-tests/enc_cann_ntt/EN13-encrypt-liboqs-cross/`（壳自 EN09）
- **结论**：**PASS** — cpu + `SIM_DIRECT=1` sim；`c` vs liboqs **max=0**；SIM Total tick **812698**；sync_audit 红线 0
- **关键修复**：INTT 矩阵由 EN09 的 ×512 改为 FIPS/Kyber ×**3303** 互逆；Host 先过 `c≡liboqs` 再接线
- **分工**：设备 SampleNTT/Prep/NTT/Matvec/Dot/INTT/Pack；Host Âᵀ 转置 + e1/e2/μ 加噪 + `t̂` ByteDecode₁₂
- **新核**：`enc_dot_real`（⟨t̂,ŷ⟩；不可 k=1 冒充 matvec）
- **文档**：STATUS / `enc-encaps-cann-ntt-sim/tasks/EN13/FEEDBACK.md` / QUEUE EN13=PASS / `enc_cann_ntt/INDEX`
- **日志**：`/opt/cursor/artifacts/enc-encaps-sim/en13-cpu.log`、`en13-sim.log`
- **下一刀**：EN14 sticky 或 EP01 Encaps 壳

---

## SIM 战役收官（同日追加）

| 刀 | 结论 | 关键数字 |
|----|------|----------|
| EN14 sticky R=8 | **PASS** | SIM wall≈1046s；tick≈6455278；每轮 c max=0 |
| EP01 Host 壳 | **PASS** | 长度门禁 |
| EP02 真调 Encrypt | **PASS** | SIM tick≈812592；自洽 c/K |
| EP03 设备哈希 | **DEFERRED_HOST** | 强完成不阻塞 |
| EP04 Encaps×liboqs | **PASS** | SIM tick≈812819；c/K max=0 |
| EP05 sticky R=16 | **PASS** | SIM wall≈2050s；tick≈12897130；每轮 c/K max=0 |

- **SIM 强完成**：Encrypt 交叉 + Encaps 交叉 + sticky 全绿；NPU 仍 `BLOCKED_UNTIL_USER`
- 日志目录：`/opt/cursor/artifacts/enc-encaps-sim/`
- QUEUE / AGENT_HANDOFF / 图谱已刷新
