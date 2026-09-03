# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-03（用户：Decrypt 独立图谱 + SoftSync toy；禁空耗催上机）

---

## ★ 给新 Agent 的 60 秒上手

1. **两条 debug 线（分开）**  
   - Encrypt/`l18_l19`：[`docs/rg-encrypt-l18.yaml`](docs/rg-encrypt-l18.yaml)；GT-1..7 SIM 未挂。  
   - **Decrypt fused**：**独立** [`docs/rg-decrypt-fused.yaml`](docs/rg-decrypt-fused.yaml) + [`graph-tests/decrypt/`](graph-tests/decrypt/INDEX.md)；近目标 = **SoftSync toy**（`Q-TOY-SOFTSYNC`）。  
2. **现状**：Decrypt DGT-1..4 toy SIM **均未挂**；stable Cloud SIM 亦未复现。Encrypt toy **没有** SoftSync，不能当 Decrypt 答案。  
3. 同时只派 1 subagent；禁自主 push/commit/开分支。  
4. 纪要：[`qa/2026-09/2026-09-03-Encrypt卡死图谱与toy近目标.md`](qa/2026-09/2026-09-03-Encrypt卡死图谱与toy近目标.md) §19。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0（当次）** | Decrypt toy DGT-1..4 **SIM 均未挂**；下一刀=更近生产体量，**不催上机** |
| **Decrypt 最终** | `Q-ULT`（NPU prod input-only 不卡且 m 对拍） |
| **Encrypt 最终** | Encrypt 图 `Q-ULT`（另线） |

**别做**：催立刻拷机跑 stable decrypt；把 Encrypt GT 未挂写成 Decrypt 已解；未授权 commit/push。

---

## ★ 当前真相（勿重复大工程）

| 项 | 状态 |
|----|------|
| **1024** | PKE×3 + KEM×3（+ Decaps-ct 专题）**stable 已齐** |
| **768** | incubating W4+glue **有条件完成**；**禁** stable-768 |
| **512** | P0–P3（W0–W4+glue）**有条件完成**；liboqs-512 KeyGen/Encaps `c`+`K`/Decaps **全绿**；**禁** stable-512 |
| **glue-c（已闭环）** | Encrypt 曾误 5 行全 η2；已补 **`r←η1=3` / `e←η2=2`**（PRF 行 stride 192） |
| **用语** | 只用 **缺项 / 补缺**（勿造同义词） |
| **额度纪律** | Rule **3/6/7** + Skill **1/2** → [`docs/engineering/Cloud-Agent额度与验收分层.md`](docs/engineering/Cloud-Agent额度与验收分层.md) |
| **领导遗留** | 方法对「有绿灯之后」有效；**第一块绿灯几个月**另论。跨域样例=**ML-DSA**；其它场景未定 |

### 512 关键路径

| 树 | 路径 |
|----|------|
| 参数卡 / 计划 | [`docs/specs/fips203-mlkem512-parameter-card.md`](docs/specs/fips203-mlkem512-parameter-card.md) · [`docs/research/MLKEM-512-从0到exp完整实现计划.md`](docs/research/MLKEM-512-从0到exp完整实现计划.md) |
| 探针 | [`ascendc-tests/ml-kem/ml-kem-512/`](ascendc-tests/ml-kem/ml-kem-512/INDEX.md) |
| incubating | [`examples/incubating/ml-kem/ml-kem-512/`](examples/incubating/ml-kem/ml-kem-512/INDEX.md) |
| glue 脚本 | [`scripts/exp_kem512_liboqs_roundtrip.sh`](scripts/exp_kem512_liboqs_roundtrip.sh) |

### 关键 SIM tick（glue-c 后，910B4）

| ID | tick | 备注 |
|----|------|------|
| D14 / E14 | **365995** / **366129** | Encrypt |
| E20 | **427927** | Encaps |
| D20 | 旧 **394978** | **待重登**（代码已随 glue-c，tick 未重测） |

---

## ★ 下一刀（实机一次搬码；512 审阅仍为 P2）

**P0（用户当次）**：借入机 **一条命令** — [`scripts/npu_kem_one_trip.sh`](scripts/npu_kem_one_trip.sh)（诊断 E1/E2 + 验收 + 教材 14 档 + 探针；自动 `BRING_BACK.tar.gz`）。清单：[`docs/engineering/实机一次搬码验收清单.md`](docs/engineering/实机一次搬码验收清单.md)。

搬码前（无 NPU）：`NPU_ONE_TRIP_MANIFEST=1` 与 `NPU_SUITE_DRY_RUN=1 bash scripts/npu_kem_one_trip.sh`  
实机：`unset ASCEND_DEVICE_ID && bash scripts/npu_kem_one_trip.sh`  
带回：`output/npu_one_trip/latest/BRING_BACK.tar.gz`（含 `[l18-trace]` + `STATUS.md`）

仅填教材表、不要 E1 时可：`bash scripts/npu_kem_textbook_perf.sh`（仍建议先 E1 再 Encaps msprof）。

用户尚未仔细审 512 实现。若用户未指定 NPU 线，**默认做审阅型收尾**，按用户当次指令收窄。

### A. 文档（优先、省额度）

| 项 | 做什么 |
|----|--------|
| 512 审阅笔记 | 用户指出问题 → 写入**当日** `qa/2026-07/…`（同日只一篇追加）+ 刷新相关 `STATUS`/`INDEX` |
| D20 tick | 可选：重跑 D20 SIM，更新 STATUS / `qa/active_sim_regress_summary.md` |
| registry | 512 六份 baseline-registry 多为骨架；incubating 绿后可按需补登记（缺计算块来源则停） |
| 教材/计划 | 仅当用户点名；768/512「有条件完成」已入卡 |

### B. 代码完善（须有依据；禁擅自改参）

| 项 | 做什么 |
|----|--------|
| 跟读 Encrypt 噪声路径 | 确认 D14/E14/E20 与参数卡 η1/η2、注释一致（glue-c 已改，防回归） |
| 注释/自包含 | 发现缺中文注释或错误路径引用 → **同轮**补；`examples/` 须活跃 customspec |
| 小修 | 编译警告、`gen_data` fixture、INDEX 过时行；**先 CPU 齐再成批 SIM** |
| 回归冒烟 | 改动触及 Encrypt/KEM 时：`USE_LIBOQS=1 bash scripts/exp_kem512_liboqs_roundtrip.sh`（至少 CPU；出口再 SIM） |

### C. 明确非本阶段（除非用户 `#…#` / 新授权）

- **`#交付#` stable-512 / stable-768**  
- 新参数组大工程、NPU、T23 多 Core、SHA3hp 替换（T21 待拍板）  
- 从 `**/frozen/**` 抄码；合 PR 策略听用户（近期常：research 推送 → 偶发合 main → 切回 research）

---

## ★ 已锁参数（512；有歧义先停）

| 项 | 值 |
|----|-----|
| 分核 | **S-1** 单 cube；**T-B2** polyvec4；**禁零垫** |
| 噪声 | **η1=3，η2=2**（Encrypt：`r` 用 η1；`e₁‖e₂` 用 η2） |
| 压缩 | **du=10，dv=4** |
| I/O | ek **800** / dk_pke **768** / dk_kem **1632** / c **768** |
| 后缀 | `-k2` |

---

## ★ 勿做

- frozen 抄码；零垫凑 3/4/6/8  
- 擅自改已锁形状/tiling/`blockDim`  
- 只报自洽绿、跳过假绿三问（对拍/交叉失败时）  
- 并行多路 SIM；文档长刷夹在未授权的大改码会话里乱开范围  
- 默认 Encaps `m` 全 0（1024 已禁；512/768 亦勿退回）

---

## ★ 最小 smoke（改码后）

```bash
# 单用例（声称通过须双模式）
cd <case_dir>
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# 512 glue（交叉）
USE_LIBOQS=1 bash scripts/exp_kem512_liboqs_roundtrip.sh

# 1024 实机（借入机；按树分卡 1/2/3，无需装 thirdparty）
# 推荐一次搬码：scripts/npu_kem_one_trip.sh（见 docs/engineering/实机一次搬码验收清单.md）
# 或分 phase：scripts/npu_kem_real_machine_suite.sh
bash run.sh -r npu -v Ascend910B4   # 未 export 时：stable→1 / incubating→2 / 探针→3
# 显式覆盖：ASCEND_DEVICE_ID=0 bash run.sh -r npu …
RUN_WITH_MSPROF=1 bash run.sh -r npu -v Ascend910B4   # 非默认：要 msprof 报告时才加

# 1024 KEM golden 后端自检（非默认）
KEM_GOLDEN_BACKEND=python bash run.sh -r cpu -v Ascend910B4   # 强制回落
KEM_GOLDEN_CROSS=1        bash run.sh -r cpu -v Ascend910B4   # liboqs ≡ python 逐字节
```

会话结束前：刷新**本文件** + 当日 `qa/` + 动过的 `INDEX.md`。
