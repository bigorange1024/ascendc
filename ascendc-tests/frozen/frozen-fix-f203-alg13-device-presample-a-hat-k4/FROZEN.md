# ⛔ 已冻结（2026-06-28）

**原路径**：`ascendc-tests/fix-f203-alg13-device-presample-a-hat-k4/`  
**角色**：Alg.13 **行 3–7 + 8–15 全链** Phase A 实验台（G+A+P+C 单 launch benchmark）

## 冻结原因

| 类别 | 说明 |
|------|------|
| **任务完成** | Phase 1 + A-v1~v4b CPU/SIM 对拍完成；tick 表见 [`SIM_BENCHMARK.md`](SIM_BENCHMARK.md)（默认 **881627**）。 |
| **路线已拆分** | 生产集成不再走「全链单 launch」；行 3–7 / 8–15 分别由专用 pass 探针承担。 |
| **A-v5 不阻塞集成** | UB d1/d2 POC 未做；单 poly 能力已在 [`pass-fix-f203-alg7-sample-ntt-k4`](../../pass-fix-f203-alg7-sample-ntt-k4/) 闭环。 |

## 继任（活跃探针）

| FIPS 段 | 探针 |
|---------|------|
| Alg.7 单 poly 模块 | [`pass-fix-f203-alg7-sample-ntt-k4`](../../pass-fix-f203-alg7-sample-ntt-k4/) |
| Alg.13 行 3–7 `Â` | [`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../../pass-fix-f203-alg13-lines3-7-a-hat-k4/) |
| Alg.13 行 8–15 `s`/`e` | [`pass-fix-f203-alg13-lines8-15-se-k4`](../../pass-fix-f203-alg13-lines8-15-se-k4/) |
| 全链 KeyGen | [`pass-fix-f203-alg13-device-keygen-k4`](../../pass-fix-f203-alg13-device-keygen-k4/) / [`exp-mlkem-f203-pke-keygen-k4`](../../../examples/incubating/exp-mlkem-f203-pke-keygen-k4/) |

**只读**：Phase A 历史 tick、A-v4 反模式结论 → [`docs/notes/F203-Alg7-PhaseA-向量化技术总结.md`](../../../docs/notes/F203-Alg7-PhaseA-向量化技术总结.md)。  
**勿 fork、勿抄码、勿跑 CI。**
