# STATUS — ER01-encrypt-shaped-2launch-skel

**结果：PASS**（CPU + SIM 不挂死）

| 项 | 值 |
|----|-----|
| 目标 | Encrypt 外形双 launch：prep 桩 + MIX 计算壳（NTT 1/3 + 生产 GATE AIC 先 WAIT4 + T06 级真 Vec MAC + INTT 复用 1/3） |
| 目录 | `graph-tests/enc_related/ER01-encrypt-shaped-2launch-skel/` |
| CPU | **PASS**，`bash run.sh -r cpu -v Ascend910B4`，exit 0，kernel wall≈1.6s |
| SIM | **PASS**，`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，exit 0，kernel wall≈9.3s，tick 45991 |
| launch | **2**：110=PREP → Sync；120=COMPUTE → Sync → 199 |
| prep | Host seed 32B；AIV `AivSampleStub` 4 轮 Muls+Add → SAMPLE_OUT；AIC 立即返回（无 CrossCore） |
| GATE brick | `AivGateRealBrickMac` int32[64]×8 轮 Mul/Add/Muls（非 X14 空转） |
| INTT flag | 复用 **1/3**；**未使用 5/7** |
| 禁令 | 无 SyncAll@AIC-Wait、无 SoftSync、未抄 Encrypt |
| stray dump | 无（已收拢至 `sim_log/`） |
| 日志 | `/opt/cursor/artifacts/ER01-cpu.log` · `/opt/cursor/artifacts/ER01-sim.log` · `/opt/cursor/artifacts/ER01-sync_audit.json` |

## TRACE

| 模式 | 序列要点 |
|------|----------|
| CPU | `110 211 311 212 312 120 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（完整） |
| SIM | `110 211 311 212 312 120 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（**缺 401**，X13 噪声） |

## sync_audit（原样保留；不得否决候选）

命令：

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  graph-tests/enc_related/ER01-encrypt-shaped-2launch-skel/{mmad_custom.cpp,aiv_func.hpp,aic_func.hpp,basic.hpp,kyber_limb6.hpp,tiling.h} \
  --format json
```

| 汇总 | 值 |
|------|-----|
| files_scanned | 6 |
| event_count | 10 |
| findings | **24**（红线 **16** + 性能 **8**） |
| 最严重 | **红线** `SYNC-02`（跨流水缺同步 / MTE2 后未同步即计算） |

### 红线（16，原样）

- [SYNC-02] aiv_func.hpp:56: 疑似 MTE2 搬入后未同步即计算: Duplicate@56 (搬入@54)
- [SYNC-02] aiv_func.hpp:59: 疑似 V 计算后未同步即 Scalar 读: Scalar@59 (计算@56)
- [SYNC-02] aiv_func.hpp:59: 变量 seed 跨流水缺同步: MTE2(MTE2_write)@L54 → S(S_read)@L59
- [SYNC-02] aiv_func.hpp:65: 变量 work 跨流水缺同步: MTE2(MTE2_write)@L57 → V(V_read)@L65
- [SYNC-02] aiv_func.hpp:66: 变量 work 跨流水缺同步: MTE2(MTE2_write)@L57 → V(V_write)@L66
- [SYNC-02] aiv_func.hpp:66: 变量 work 跨流水缺同步: MTE2(MTE2_write)@L57 → V(V_read)@L66
- [SYNC-02] mmad_custom.cpp:102: 疑似 V 计算后未同步即 Scalar 读: Scalar@102 (计算@101)
- [SYNC-02] aiv_func.hpp:126: 变量 sample 跨流水缺同步: MTE2(MTE2_write)@L122 → S(S_read)@L126
- [SYNC-02] aiv_func.hpp:187: 疑似 MTE2 搬入后未同步即计算: Muls@187 (搬入@183)
- [SYNC-02] aiv_func.hpp:187: 疑似 MTE2 搬入后未同步即计算: Muls@187 (搬入@184)
- [SYNC-02] aiv_func.hpp:187: 变量 b 跨流水缺同步: MTE2(MTE2_write)@L184 → V(V_write)@L187
- [SYNC-02] aiv_func.hpp:187: 变量 b 跨流水缺同步: MTE2(MTE2_write)@L184 → V(V_read)@L187
- [SYNC-02] aiv_func.hpp:188: 变量 a 跨流水缺同步: MTE2(MTE2_write)@L183 → V(V_read)@L188
- [SYNC-02] aiv_func.hpp:189: 变量 acc 跨流水缺同步: MTE2(MTE2_write)@L176 → V(V_write)@L189
- [SYNC-02] aiv_func.hpp:189: 变量 acc 跨流水缺同步: MTE2(MTE2_write)@L176 → V(V_read)@L189
- [SYNC-02] aiv_func.hpp:233: 疑似 V 计算后未同步即 Scalar 读: Scalar@233 (计算@232)

### 性能（8，原样）

- [SYNC-09] kyber_limb6.hpp:13: PipeBarrier\<PIPE_ALL\> 粒度过粗
- [SYNC-09] mmad_custom.cpp:50: PipeBarrier\<PIPE_ALL\> 粒度过粗
- [SYNC-09] mmad_custom.cpp:58: PipeBarrier\<PIPE_ALL\> 粒度过粗
- [SYNC-09] aiv_func.hpp:68: PipeBarrier\<PIPE_ALL\> 粒度过粗
- [SYNC-09] aiv_func.hpp:85: PipeBarrier\<PIPE_ALL\> 粒度过粗
- [SYNC-09] mmad_custom.cpp:96: PipeBarrier\<PIPE_ALL\> 粒度过粗
- [SYNC-09] mmad_custom.cpp:107: PipeBarrier\<PIPE_ALL\> 粒度过粗
- [SYNC-09] aiv_func.hpp:194: PipeBarrier\<PIPE_ALL\> 粒度过粗

完整 JSON：`/opt/cursor/artifacts/ER01-sync_audit.json`

## 学到的一条

Encrypt 外形「prep launch（无 CrossCore）→ compute MIX（生产 GATE 4/8 + 真积木）」在 SIM 上仍不挂；卡死主因仍更可能在生产体量/真机路径，而非「双 launch 外形」本身（印证 X6 / X15 边界向 enc_related 平移后的首刀结果）。

## 反馈块（主控）

```
ER01: PASS
dir: graph-tests/enc_related/ER01-encrypt-shaped-2launch-skel/
cpu: PASS exit0 wall≈1.6s
sim: PASS exit0 wall≈9.3s tick=45991 (缺401/X13)
sync_audit: findings=24 红线=16 性能=8；最严重=SYNC-02 跨流水缺同步
blocked: 无
learned: Encrypt 外形 2launch+生产GATE+真MAC 在 SIM 仍不挂；卡死更偏生产体量/NPU
```
