# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-07-31（**T2-npu-env P0 已合入**：`env.sh`/`add_custom`；`ASCEND_DEVICE_ID` 缺省 **1**；借入机拉 main 测 `-r npu`）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：优先看当前 PR / `research/formal-lang-dag`；开新改动先 `git pull`。  
2. Cloud 首次：`bash scripts/clone-thirdparty.sh`（含 liboqs；须保留完整 build 树以便编 PKE ref）。  
3. **下一任务方向**：**ML-KEM-512 审阅与收尾**（文档 + 小修）；非 stable 晋级。  
4. 先读本文件 → [`qa/TODO.md`](qa/TODO.md) → [`qa/2026-07/2026-07-28-T18-PKE-ref链接修复.md`](qa/2026-07/2026-07-28-T18-PKE-ref链接修复.md) / 27 日纪要。  
5. 写 AscendC 前：Rule + [`ascendc-engineering-notes`](.cursor/skills/ascendc-engineering-notes/SKILL.md)（含 §8.1 排程）。

### 刚关闭

| 项 | 说明 |
|----|------|
| **T18** | PKE helper encrypt/decrypt 链接：链 `ml_kem_1024_ref` `.o` + fips202 shim + `liboqs-internal.a`；**非改名问题** |

### 待办快照（新增，非本阶段主线）

| 项 | 说明 |
|----|------|
| **T2-npu-env** | **P0 已扩**：`add_custom` + toy-mix + **PKE device 三探针**（alg13 keygen / alg14 encrypt / alg15 decrypt）。口径：`REPO_ROOT/env.sh`；`ASCEND_DEVICE_ID` 实机缺省 1、SIM=0。借入机对各目录 `bash run.sh -r npu -v Ascend910B4` |

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

## ★ 下一刀（文档 + 代码完善；默认 P0）

用户尚未仔细审 512 实现。新 Agent **默认做审阅型收尾**，按用户当次指令收窄。

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
```

会话结束前：刷新**本文件** + 当日 `qa/` + 动过的 `INDEX.md`。
