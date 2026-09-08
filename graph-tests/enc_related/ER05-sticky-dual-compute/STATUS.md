# STATUS — ER05-sticky-dual-compute

**结果：PASS**（Host **3 launch**：PREP→COMPUTE→COMPUTE，同 GM workspace；核内保持 ER04；CPU + SIM 不挂；sync_audit **红线 0**）

| 项 | 值 |
|----|-----|
| 目标 | 验「多 launch 粘性」（脏 workspace / CrossCore 残留 / 二次进 MIX）是否足以 SIM 复现挂 |
| 目录 | `graph-tests/enc_related/ER05-sticky-dual-compute/`（自 ER04 复制脚手架；**只改 Host**） |
| 已锁 Host | **3** launch：110=PREP → 120=COMPUTE#1 → 121=COMPUTE#2 → 199；第二轮不得再走 PREP |
| 已锁核参 | 同 ER04：GATE `256×32`；Cube 几何 `16×32×32`；`kCubeRounds=16`（**未改核**） |
| TRACE 约定 | 每轮 launch 前 Host 清 TRACE 槽；用 `120`/`121` 分段区分两轮 COMPUTE（见 `trace_map.md`） |
| CPU | **PASS**，`bash run.sh -r cpu -v Ascend910B4`，exit 0，kernel wall≈**2.603s** |
| SIM | **PASS**，`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，exit 0，kernel wall≈**25.420s**，tick **164460** |
| 同步纪律 | 核内保持 ER02/ER04：GM→UB 后 EnQue/DeQue；`PipeBarrier<PIPE_V>`；VECIN/VECOUT |
| INTT flag | 复用 **1/3**；**未使用 5/7** |
| 禁令 | 无 SyncAll@AIC-Wait、无 SoftSync、未抄 Encrypt、无假循环、未改核锁参、未连 NPU、未 commit/push |
| stray dump | 无（收拢至 `sim_log/`） |
| 日志 | `/opt/cursor/artifacts/ER05-cpu.log` · `/opt/cursor/artifacts/ER05-sim.log` · `/opt/cursor/artifacts/ER05-sync_audit.json` |

## TRACE

| 模式 | 序列要点 |
|------|----------|
| CPU | `110 211 311 212 312 120 …完整 COMPUTE… 121 …完整 COMPUTE… 199`（两轮均含 401） |
| SIM | `110 211 311 212 312 120 …COMPUTE#1（缺 401/X13）… 121 …COMPUTE#2（含 401）… 199` |

## sync_audit

命令：

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  graph-tests/enc_related/ER05-sticky-dual-compute/{mmad_custom.cpp,aiv_func.hpp,aic_func.hpp,basic.hpp,kyber_limb6.hpp,tiling.h} \
  --format json
```

| 汇总 | 值 |
|------|-----|
| findings | **11**（红线 **0** + 性能 11） |
| 最严重 | 性能 `SYNC-09` / `SYNC-11` |

### 红线

**无**。

### 性能（保留，非门禁；已标注）

- [SYNC-09]×10：多处 `PipeBarrier<PIPE_ALL>` 粒度过粗；`AivSampleStub::Process` 连续 barrier 偏多
- [SYNC-11]×1：`aiv_func.hpp` GATE acc 写回前 `EnQue→DeQue`（标准 CopyOut 形；脚本启发式「无计算」）

完整 JSON：`/opt/cursor/artifacts/ER05-sync_audit.json`

## 学到的一条

同 workspace 连续两轮 COMPUTE（粘性双 launch）后 CPU/SIM 仍不挂；SIM wall≈25.4s / tick≈164k（约 2×ER04），说明 **近生产体量下单纯「二次进 MIX + 脏 workspace」仍不足以在 CAModel 复现生产挂死**。

## 反馈块（主控）

```
ER05: PASS
dir: graph-tests/enc_related/ER05-sticky-dual-compute/
cpu: PASS exit0 wall≈2.603s
sim: PASS exit0 wall≈25.420s tick=164460 (COMPUTE#1 缺401/X13；#2 有401)
sync_audit: 红线=0；性能 SYNC-09×10 + SYNC-11×1（已标注）
blocked: 无
learned: 同 ws 粘性双 COMPUTE 不挂；多 launch 粘性 alone 未复现生产挂死
```
