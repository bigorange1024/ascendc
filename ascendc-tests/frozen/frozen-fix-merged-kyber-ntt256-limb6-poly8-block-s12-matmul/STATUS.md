# frozen-fix-merged-kyber-ntt256-limb6-poly8-block-s12-matmul — **路线废弃中止**

| 字段 | 内容 |
|------|------|
| **状态** | **废弃冻结**（2026-06-11） |
| **路线** | NTT Stage1(`mmad`) + Stage2 高阶 `Matmul<>` 两段式 |
| **CPU** | ✓ 两段式 `max_diff=0` |
| **SIM** | ✗ Stage2 `pem_lsu invalid ldst addr`（Stage1 ~5s ✓） |
| **替代** | `frozen-fix-merged-kyber-ntt256-limb6-poly8-block-s123`（`AicMmad`，已冻结） |
| **日志** | `logs/s12-sim-final-20260611.log` |

勿 fork。见 [frozen/INDEX.md](../INDEX.md)、[qa 冻结纪要](../../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#ntt-matmul路线废弃冻结)。
