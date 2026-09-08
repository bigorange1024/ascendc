# STATUS — ER03-gate-realbrick-volume

**结果：PASS**（GATE 真积木 **256×32**；CPU + SIM 不挂；sync_audit **红线 0**）

| 项 | 值 |
|----|-----|
| 目标 | 在 ER01 外形 + ER02 同步纪律下，把 GATE 真 Vec MAC 加压到近生产体量，验 X17（AIC WAIT(4) 时空等） |
| 目录 | `graph-tests/enc_related/ER03-gate-realbrick-volume/`（自 ER01 复制脚手架后改参） |
| 已锁 MAC | `kMacElems=256`，`kMacRounds=32`（真 Mul/Add/Muls；禁空转） |
| Cube | 仍 `C[16,32]=A[16,32]@B[32,32]` 极轻（未加压） |
| CPU | **PASS**，`bash run.sh -r cpu -v Ascend910B4`，exit 0，kernel wall≈**1.635s** |
| SIM | **PASS**，`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，exit 0，kernel wall≈**10.071s**，tick **63849** |
| launch | **2**：110=PREP → Sync；120=COMPUTE → Sync → 199 |
| 同步纪律 | 保持 ER02：GM→UB 后 **EnQue/DeQue**；`Duplicate`/V 后接 Scalar 前 **`PipeBarrier<PIPE_V>`**；输入 **VECIN** / 输出 **VECOUT** |
| INTT flag | 复用 **1/3**；**未使用 5/7** |
| 禁令 | 无 SyncAll@AIC-Wait、无 SoftSync、未抄 Encrypt、无假循环加压、未擅自改锁参、未连 NPU |
| stray dump | 无（收拢至 `sim_log/`） |
| 日志 | `/opt/cursor/artifacts/ER03-cpu.log` · `/opt/cursor/artifacts/ER03-sim.log` · `/opt/cursor/artifacts/ER03-sync_audit.json` |

## TRACE

| 模式 | 序列要点 |
|------|----------|
| CPU | `110 211 311 212 312 120 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（完整） |
| SIM | `110 211 311 212 312 120 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（**缺 401**，同 ER01/ER02 X13 噪声） |

## sync_audit

命令：

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  graph-tests/enc_related/ER03-gate-realbrick-volume/{mmad_custom.cpp,aiv_func.hpp,aic_func.hpp,basic.hpp,kyber_limb6.hpp,tiling.h} \
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

完整 JSON：`/opt/cursor/artifacts/ER03-sync_audit.json`

## 学到的一条

近生产体量真 Vec MAC（256×32，相对 ER01 的 64×8）在 SIM 下仍不挂（tick≈64k / wall≈10s），说明 **仅 GATE 真积木加压不足以在 CAModel 复现生产挂死**；X17 仍待真机或更大 Cube/全链路加压继任刀验证。

## 反馈块（主控）

```
ER03: PASS
dir: graph-tests/enc_related/ER03-gate-realbrick-volume/
cpu: PASS exit0 wall≈1.635s
sim: PASS exit0 wall≈10.071s tick=63849 (缺401/X13)
sync_audit: 红线=0；性能 SYNC-09×10 + SYNC-11×1（已标注）
blocked: 无
learned: GATE 真 MAC 加压到 256×32 后 CPU/SIM 仍不挂；SIM 绿≠生产挂死根因已排除，继任需真机或 Cube/全链加压
```
