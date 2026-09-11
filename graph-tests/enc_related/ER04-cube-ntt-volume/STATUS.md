# STATUS — ER04-cube-ntt-volume

**结果：PASS**（NTT/INTT 各真 Cube **×16**；GATE 保持 **256×32**；CPU + SIM 不挂；sync_audit **红线 0**）

| 项 | 值 |
|----|-----|
| 目标 | 在 ER03 壳上对 AIC 真 Mmad 多轮加压，验「仅 GATE 不够 → 真 Cube 体量」是否足以 SIM 复现挂 |
| 目录 | `graph-tests/enc_related/ER04-cube-ntt-volume/`（自 ER03 复制脚手架后改 Cube 轮数） |
| 已锁 GATE | `kMacElems=256`，`kMacRounds=32`（保持，不回退） |
| 已锁 Cube | 几何 `C[16,32]=A[16,32]@B[32,32]`；`kCubeRounds=16`（NTT 与 INTT 各一轮 `Init`+`Process×16`） |
| CPU | **PASS**，`bash run.sh -r cpu -v Ascend910B4`，exit 0，kernel wall≈**1.615s** |
| SIM | **PASS**，`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，exit 0，kernel wall≈**12.725s**，tick **85889** |
| launch | **2**：110=PREP → Sync；120=COMPUTE → Sync → 199 |
| 同步纪律 | 保持 ER02：GM→UB 后 **EnQue/DeQue**；`Duplicate`/V 后接 Scalar 前 **`PipeBarrier<PIPE_V>`**；输入 **VECIN** / 输出 **VECOUT** |
| INTT flag | 复用 **1/3**；**未使用 5/7** |
| 禁令 | 无 SyncAll@AIC-Wait、无 SoftSync、未抄 Encrypt、无假循环加压、未擅自改锁参、未连 NPU |
| stray dump | 无（收拢至 `sim_log/`） |
| 日志 | `/opt/cursor/artifacts/ER04-cpu.log` · `/opt/cursor/artifacts/ER04-sim.log` · `/opt/cursor/artifacts/ER04-sync_audit.json` |

## TRACE

| 模式 | 序列要点 |
|------|----------|
| CPU | `110 211 311 212 312 120 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（完整） |
| SIM | `110 211 311 212 312 120 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（**缺 401**，同 ER01–ER03 X13 噪声） |

## sync_audit

命令：

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  graph-tests/enc_related/ER04-cube-ntt-volume/{mmad_custom.cpp,aiv_func.hpp,aic_func.hpp,basic.hpp,kyber_limb6.hpp,tiling.h} \
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

完整 JSON：`/opt/cursor/artifacts/ER04-sync_audit.json`

## 学到的一条

相对 ER03（tick≈64k / wall≈10s），本刀 NTT+INTT 各 16×真 Mmad 后 SIM tick≈**86k** / wall≈**12.7s**，仍不挂；说明 **近生产 GATE + 轻几何 Cube×16 仍不足以在 CAModel 复现生产挂死**。

## 反馈块（主控）

```
ER04: PASS
dir: graph-tests/enc_related/ER04-cube-ntt-volume/
cpu: PASS exit0 wall≈1.615s
sim: PASS exit0 wall≈12.725s tick=85889 (缺401/X13)
sync_audit: 红线=0；性能 SYNC-09×10 + SYNC-11×1（已标注）
blocked: 无
learned: GATE 256×32 + NTT/INTT 各 Cube×16 后 CPU/SIM 仍不挂；SIM 维真 Cube 加压未复现挂死
```
