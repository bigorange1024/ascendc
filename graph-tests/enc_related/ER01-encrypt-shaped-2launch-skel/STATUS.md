# STATUS — ER01-encrypt-shaped-2launch-skel（含 ER02）

**结果：PASS**（ER01 外形 + **ER02 SYNC-02 清零**；CPU + SIM 不挂死）

| 项 | 值 |
|----|-----|
| 目标 | Encrypt 外形双 launch + cannbot `SYNC-02` 红线清零（禁否决脚本候选） |
| 目录 | `graph-tests/enc_related/ER01-encrypt-shaped-2launch-skel/`（ER02 就地修） |
| CPU | **PASS**，`bash run.sh -r cpu -v Ascend910B4`，exit 0，kernel wall≈1.65s |
| SIM | **PASS**，`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，exit 0，kernel wall≈7.0s，tick **46008** |
| launch | **2**：110=PREP → Sync；120=COMPUTE → Sync → 199 |
| ER02 修法 | GM→UB 后 **EnQue/DeQue**；`Duplicate`/V 算后接 `SetValue`/`GetValue` 前 **`PipeBarrier<PIPE_V>`**；输入队列 **VECIN**、输出 **VECOUT** |
| 未采用 | 否决红线；本刀未引 `HardEvent::SetFlag/WaitFlag`（优先 EnQue/DeQue） |
| INTT flag | 复用 **1/3**；**未使用 5/7** |
| 禁令 | 无 SyncAll@AIC-Wait、无 SoftSync、未抄 Encrypt |
| stray dump | 无（收拢至 `sim_log/`） |
| 日志 | `/opt/cursor/artifacts/ER02-cpu.log` · `/opt/cursor/artifacts/ER02-sim.log` · `/opt/cursor/artifacts/ER02-sync_audit.json` |

## TRACE

| 模式 | 序列要点 |
|------|----------|
| CPU | `110 211 311 212 312 120 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（完整） |
| SIM | `110 211 311 212 312 120 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（**缺 401**，X13 噪声） |

## sync_audit（ER02 后）

命令：

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  graph-tests/enc_related/ER01-encrypt-shaped-2launch-skel/{mmad_custom.cpp,aiv_func.hpp,aic_func.hpp,basic.hpp,kyber_limb6.hpp,tiling.h} \
  --format json
```

| 汇总 | ER01 基线 | ER02 后 |
|------|-----------|---------|
| findings | 24（红线 **16** + 性能 8） | **11**（红线 **0** + 性能 11） |
| 最严重 | 红线 `SYNC-02` | 性能 `SYNC-09` / `SYNC-11` |

### 红线

**无**（原 16× SYNC-02 已清）。

### 性能（保留，非门禁；已标注）

- [SYNC-09] 多处 `PipeBarrier<PIPE_ALL>` 粒度过粗；`AivSampleStub::Process` 连续 barrier 偏多
- [SYNC-11] `aiv_func.hpp` acc 写回前 `EnQue→DeQue`（标准 CopyOut 形；脚本启发式「无计算」）

完整 JSON：`/opt/cursor/artifacts/ER02-sync_audit.json`（副本可放 `.cannbot/mlkem-pke-encrypt/tmp/`）

## 学到的一条

SIM 绿不能否决 cannbot 红线（X24）；就地补 EnQue/DeQue + `PIPE_V` 后红线可清零且 CPU/SIM 仍不挂——卡死主因仍更可能在生产体量/真机路径（继任 ER03）。

## 反馈块（主控）

```
ER02: PASS
dir: graph-tests/enc_related/ER01-encrypt-shaped-2launch-skel/
cpu: PASS exit0 wall≈1.65s
sim: PASS exit0 wall≈7.0s tick=46008 (缺401/X13)
sync_audit: 红线=0；性能 SYNC-09×10 + SYNC-11×1（已标注）
blocked: 无（NPU 他 agent 占用，本刀未上机）
learned: EnQue/DeQue+PIPE_V 可清 SYNC-02 且不破坏外形不挂；下一刀 ER03 近生产体量
```
