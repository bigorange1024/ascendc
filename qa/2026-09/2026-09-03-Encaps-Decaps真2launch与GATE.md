# 2026-09-03 — Encaps/Decaps 真 2-launch（prep∈MIX + GATE）

关键字：**2 Host launch** · **prep_ntt|l18** · **GATE 4/8** · **Encrypt prep∈MIX 可行** · **Phase-D chain_ntt|intt** · 否决默认 3-launch · **实机未复验**

> **对照基准**：仍相对 `origin/main` 的 fused 双 Cube；承接 [09-02](2026-09-02-实机l18粘性与TRACE-107002.md) 的 TRACE 修 +「每 MIX 一轮 Cube」。  
> **本日状态词**：**有条件完成**（Cloud CPU+SIM 对拍绿；**差**实机粘性加压）。  
> **勿入库 notes**：实机未绿前，GATE/prep∈MIX 经验只留 qa；勿写成「平台通则」。

---

## 1. 用户拍板

否决 09-02 默认 **Host 3-launch**（AIV prep | ntt | l18）：prep 无 Cube，不应独占 launch。要求 Encaps / Decaps **真 2 Host launch**（每 MIX 一轮 Cube）：

| 段 | Launch-1 | Launch-2 |
|----|----------|----------|
| Encaps | `f203_kem_enc_prep_ntt`（头+Â/CBD + NTT） | `l18(ySrc=null)` |
| Decaps Phase-D | `f203_decrypt_g4_chain_ntt` | `f203_decrypt_g4_chain_intt` |
| Decaps Phase-E | `f203_kem_dec_phase_e_prep_ntt` | `l18(ySrc=null)+FO` |

回退（对照 main / 调试）：

| env | 含义 |
|-----|------|
| `F203_ENCAPS_SPLIT_PREP` / `F203_DECAPS_SPLIT_PREP` | 回 09-02 的 3-launch |
| `F203_ENCAPS_FUSED_L18` / `F203_DECAPS_FUSED_L18` / `F203_DECRYPT_FUSED` | 回 main 双 Cube 融合 |
| `F203_ENCAPS_PREP_MIX_ONLY` | 诊断：MIX 仅 prep，再独立 `ntt_y` |

---

## 2. 相对 main：改了什么 / 什么是 bug

| 相对 main | 说明 |
|-----------|------|
| **坐实 bug** | 仅 TRACE `107002`（09-02 已修 `acl_session.hpp`） |
| **编排重构** | Encaps/Phase-E/Phase-D 默认改为 2-launch；**不是**「main fused 在 SIM 上算错」的修复 |
| **算子范围** | 仅 `stable-…-kem-encaps-k4`、`stable-…-kem-decaps-k4` + 共享 `acl_session`；未改其它 stable / incubating / 探针 |

搬码上机最小集：上述两用例树 + `library/shared/acl_session/acl_session.hpp`；须 `FORCE_REBUILD` + 单卡 reset。

---

## 3. 实验中踩坑（未升 notes）

### 3.1 假绿三问（prep_ntt 曾红时）

1. **golden 同源？** 否 — K 对、c 错 → 头 OK、Encrypt 体坏。  
2. **权威交叉？** 本轮仅 Cloud SIM 自洽；实机未做。  
3. **跨核 Sync？** **是**：同核 prep 后直接 NTT(1/3)、无 GATE → SIM `c` max≈250；补 SoftSyncArrive + GATE(4/8) 后绿。

### 3.2 隔离实验（订正误判）

| 路径 | 结果 |
|------|------|
| `PREP_MIX_ONLY`：MIX 仅 prep → `ntt_y` → l18 | **PASS** |
| `prep_ntt` + GATE → l18 | **PASS** |
| 无 GATE 的 prep_ntt | **FAIL** |

**订正**：不是「Encrypt Â/SHAKE 不能进 MIX」；是 **prep 后开 Cube 前须 GATE**（对齐 Decrypt fused / Phase-D）。  
CPU 孪生路径不跑 `prep_ntt`（仍五段），**不能**用 CPU PASS 验收 prep_ntt。

### 3.3 其它工程经验（短句）

- Decaps SIM 改 host 默认后若未 `FORCE_REBUILD`，会静默跑旧 3-launch 却仍 PASS → **日志须见 `prep_ntt` / `chain_ntt` 才算当前默认**。  
- Phase-D chain 与 fused 同库时 su_dot ROM 符号冲突 → 宏改名；shim 须拷 `intt_w`。  
- CBD/Â 分片在 MIX `blockDim=1` 时用参数 `blockIdx`（subBlock），勿盲信 `GetBlockIdx()`。

---

## 4. Cloud 验收计数（当前默认真 2-launch）

| 用例 | CPU | SIM（有证据力） |
|------|-----|-----------------|
| Encaps `prep_ntt`→`l18` | ≥1 PASS | **1** PASS（`encaps_sim_2launch_gate.log`） |
| Decaps D+E（D chain + E prep_ntt） | ≥1 PASS | **1** PASS（`decaps_sim_2launch_force.log`，强制重编后） |

另有诊断 / 对照 / 未重编误跑，**不计入**「当前默认已验」。本轮未重跑 Encaps→Decaps roundtrip。

---

## 5. 实机下一刀（用户将做）

1. 取本分支工作区（勿只拉 main）。  
2. 同步两 stable + `acl_session.hpp`。  
3. `KEM_*_FORCE_REBUILD=1`，单卡 reset。  
4. Encaps↔Decaps 多轮加压；可选 `F203_L18_TRACE=1`（应不再假 107002）。  
5. 对照：偶发用 `F203_*_FUSED*=1` 看粘性是否复现。

**未声称**：粘性已消。

---

## 6. 实机反馈（用户，同日续）

| 观察 | 解读 |
|------|------|
| **不加** `FORCE_REBUILD=1` → **100% 卡死** | `run.sh` 默认 `KEM_*_SKIP_REBUILD=1`，会复用旧 `out_prod_npu` 里 **main 融合双 Cube** 二进制 → 仍走粘性路径。**不是**「新代码不加 FORCE 也必挂」 |
| **加** FORCE 后能跑完 | 新 2-launch 二进制已上机 |
| Encaps 多轮 **PASS** | 新 Encaps 路径正确性目前绿；粘性是否彻底消仍看更长加压 |
| Decaps **恒 FAIL**，`K.bin max_abs_diff=131` | K 整段错（非噪声）。常见：Phase-D `m'` 错 → FO 隐式拒绝 → `K=J(z‖c)` 对不上 accept golden；或 Phase-E 重加密错 |

### 假绿三问（Decaps NPU）

1. golden 与本次 `gen_data` 是否同一次 run.sh？日志是否 **`chain_ntt` / `prep_ntt`**（不是 `device_fused` / 三 launch）？  
2. 同机 **CPU** 同目录是否 PASS？  
3. 正交对照（须 FORCE 编过一次后可改 env 再跑）：

```bash
# A：Phase-D 回 fused，E 仍默认 prep_ntt
KEM_DECAPS_FORCE_REBUILD=1 F203_DECRYPT_FUSED=1 bash run.sh -r npu -v Ascend910B4

# B：D 默认 chain，E 回 3-launch
F203_DECAPS_SPLIT_PREP=1 bash run.sh -r npu -v Ascend910B4

# C：D+E 都回 main 融合（对照「旧正确性」）
F203_DECRYPT_FUSED=1 F203_DECAPS_FUSED_L18=1 bash run.sh -r npu -v Ascend910B4
```

| 若 | 则优先查 |
|----|----------|
| A 绿、默认红 | Phase-D `chain_*` 实机 |
| B 绿、默认红 | Phase-E `prep_ntt` 实机 |
| C 绿、默认红 | 2-launch 接缝；C 若也红则 fixture/环境 |
| 默认日志已是 chain+prep_ntt 且 CPU 绿 | NPU 同步/可见性（SoftSync/GATE） |

### 实机续报（Encaps 粘性未消）

| 观察 | 解读 |
|------|------|
| `FORCE_REBUILD=1` 后 Encaps 仍多轮卡死 | 分别约 **5 / 3 / 4** 次后挂 |
| **两种挂点（用户确认）** | (A) 最后打印 `launch 2 f203_encrypt_l18_l19 (ySrc=null)`；(B) 最后打印 `launch 1 f203_kem_enc_prep_ntt (prep+NTT, one Cube)` |
| Host 打印语义（`main_kem_encaps.cpp`） | stderr `launch N …` 在 **ACLRT_LAUNCH 之前**；`[npu_launch] … duration_us=` 仅在 **SynchronizeStream 返回后** 才打。故「最后一行 = launch 文案」= 卡在该核的 **Launch 或 Sync**，该核尚未完成 |
| (A) 含义 | launch1 已跑完（应已有 `prep_ntt` 的 `[npu_launch]`）；卡在 **l18** Launch/Sync |
| (B) 含义 | 卡在 **prep_ntt** Launch/Sync；**尚未**打印 launch2。prep_ntt = **单 Cube** MIX → 挂点**不需要**同核双 Cube |
| 总含义 | **否定**「只卡第二段 l18」；**否定**「拆双 Cube 即消」。更像 **跨次 run 设备态累积**，下一发 **任一 MIX** 撞脏态 |
| 仍成立 | 不加 FORCE → 吃旧 fused → 必挂；FORCE 后能跑完若干轮且曾 PASS |

**修订假设（待验，勿升 notes）**：

1. ~~仅第二 MIX / 仅 l18~~ → **已证伪为充分条件**（(B) 可挂在单 Cube 的 prep_ntt）。  
2. ~~仅同核双 Cube~~ → **已证伪为充分条件**（默认路径每 launch 本就一轮 Cube，仍粘）。  
3. 优先：**跨进程/跨次 launch 的 NPU 残留**（Finalize/同卡污染/Cube·CrossCore），与「本核是否双 Cube」弱相关。  
4. 两核都含 MIX+CrossCore；脏态下**谁先被 launch 谁挂**（故 (A)/(B) 可交替出现）。  
5. `F203_L18_TRACE=1`：**仅 (A)** 有用；(B) 看是否打出 `prep_ntt` 的 `[npu_launch]`（无则死在该 Sync）。

**建议下一刀（实机）**：

```bash
# 1）连跑至挂：记录最后一行 + 是否已打出 npu_launch duration（区分 launch 后死等 vs 未进 sync）
KEM_ENCAPS_FORCE_REBUILD=1 F203_L18_TRACE=1 bash run.sh -r npu -v Ascend910B4

# 2）Host 3-launch：看挂点是 ntt_y / l18 / prep（无 prep∈MIX）
F203_ENCAPS_SPLIT_PREP=1 bash run.sh -r npu -v Ascend910B4

# 3）旧 fused 基线
F203_ENCAPS_FUSED_L18=1 bash run.sh -r npu -v Ascend910B4

# 4）KeyGen 多轮（有 MIX、无 Encaps l18）
# 5）每次跑前是否单卡 reset？若「每轮 reset 则永不挂、不 reset 则 N 轮挂」→ 强支持设备残留假说
```

| 若 | 则 |
|----|----|
| 默认在 prep_ntt **或** l18 都挂 | 跨次残留 / 凡 MIX；继续对 KeyGen、SPLIT |
| SPLIT 只挂 ntt_y/l18、从不挂 AIV prep | 焦点在 **MIX+Cube**，AIV_ONLY prep 干净 |
| **每轮 reset 不挂、累积跑挂** | 残留假说；治本靠 Finalize/reset 策略或找未释放资源 |
| KeyGen 多轮不挂 | Encaps 特有（LUT/ws/双 launch 接缝等） |
