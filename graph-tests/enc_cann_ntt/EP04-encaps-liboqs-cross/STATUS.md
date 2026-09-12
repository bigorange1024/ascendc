# EP04-encaps-liboqs-cross · STATUS

> 日期：2026-09-11  
> 结论：**PASS**（CPU + SIM；`c`/`K` vs liboqs Encaps **max=0**）

| 模式 | exit | wall | tick | c/K max |
|------|------|------|------|---------|
| CPU | 0 | **1.814s** | — | **0**（liboqs_kem_vs） |
| SIM | 0 | **121.261s** | **812819** | **0** |

Glue：`scripts/liboqs_kem_vs_ascendc_verify.py --stage encaps`。  
日志：`ep04-cpu.log` / `ep04-sim.log`。禁 npu。
