# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-07-31（**T2-npu-env**：1024 探针×7 + stable×7 上机口径；`msprof_run.sh`；KEM golden 去 liboqs 硬依赖）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`main`**（2026-07-31 晚已推 1024 上机口径）；开新改动先 `git pull`。  
2. Cloud 首次：`bash scripts/clone-thirdparty.sh`（含 liboqs；须保留完整 build 树以便编 PKE ref）。  
   —— 注意：**1024 的 14 个用例现在没有 liboqs 也能跑**（KEM golden 会回落 python），但**有 liboqs 才有权威交叉**，Cloud 仍建议装。  
3. **下一任务方向**：见下「★ 接手清单」。用户口头指定优先级时以用户为准。  
4. 先读本文件 → [`qa/TODO.md`](qa/TODO.md) → [`qa/2026-07/2026-07-31-借入NPU机体检回填.md`](qa/2026-07/2026-07-31-借入NPU机体检回填.md)（**当日全量真相**）。  
5. 写 AscendC 前：Rule + [`ascendc-engineering-notes`](.cursor/skills/ascendc-engineering-notes/SKILL.md)（含 §8.1 排程）。

### 刚关闭

| 项 | 说明 |
|----|------|
| **T18** | PKE helper encrypt/decrypt 链接：链 `ml_kem_1024_ref` `.o` + fips202 shim + `liboqs-internal.a`；**非改名问题** |

### 待办快照（新增，非本阶段主线）

| 项 | 说明 |
|----|------|
| **T2-npu-env** | **1024 全覆盖**：探针 PKE×3+KEM×4 与 **stable 七算子**都走 `${REPO_ROOT}/scripts/env.sh`、`ASCEND_DEVICE_ID` npu 缺省 1 / SIM 强制 0。借入机对各目录 `bash run.sh -r npu -v Ascend910B4` 即可，无需先装 thirdparty |
| **实机 msprof** | `RUN_WITH_MSPROF` 原只在 `add_custom` 生效 → 抽出 [`scripts/msprof_run.sh`](scripts/msprof_run.sh)，接入 14 个用例；**默认不采集**，`RUN_WITH_MSPROF=1` 才在 sim/npu 走 `msprof op`，产物落 `prof_<mode>/<bin>/` |
| **零 thirdparty golden** | LUT 头三级回退（env → 仓库根私有 → 交付树 vendored）；KEM 侧新增 [`library/shared/f203_kem_ref/kem_ref.py`](library/shared/f203_kem_ref/kem_ref.py)：liboqs 优先，缺失回落仓内已验证 PKE golden + SHA3，Decaps stash 缺失按 `SEED_D` 自举。`KEM_GOLDEN_CROSS=1` 实测两路逐字节一致 |

---

## ★ 接手清单（2026-07-31 晚交接；按此挑，勿另起大工程）

**刚落地的一版**（main，已推）：1024 的 **7 探针 + 7 stable** 全部对齐上机口径；KEM golden 去掉
liboqs 硬依赖；用例软链相对化。WSL 三轮全绿证据见当日 qa §11.6，**勿重跑整树**。

| 优先 | 项 | 做什么 | 注意 |
|------|----|--------|------|
| **P0** | 实机 `-r npu` 回填 | 用户在借入机逐目录跑 `bash run.sh -r npu -v Ascend910B4`（设备号缺省 1）；Agent 侧负责**读 log 定位**、修问题、补当日 qa | 实机**无 thirdparty**：KEM 应打印 `golden … via python`；若报缺 `liboqs_kem_ref` 说明回落没走通，属 bug |
| **P1** | **T2-npu-link** | 512 / 768 / incubating 剩 12 处 `ln -sfn` 绝对路径 → `ln -sfnr` | 改哪档就把哪档 CPU 跑一遍自验；别一次性改完不验 |
| **P1** | 512 / 768 上机口径 | 若用户要在实机跑 512/768，比照 1024 做同样四件事：`env.sh` / `ASCEND_DEVICE_ID` / `msprof_run.sh` / golden 去 thirdparty | 1024 的 `run.sh` env 段可直接作模板；KEM golden 复用 `library/shared/f203_kem_ref`（现仅接了 1024，k=2/3 需按参数差清单确认） |
| **P2** | 512 审阅收尾 | 原下一刀（见下节 A/B），未开工 | 与上面的 NPU 线互不冲突，看用户当次指令 |

**别做**：`#交付#` stable-512/768（须用户点名）；把 1024 那套 env 改动往 frozen 树里搬。

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

## ★ 下一刀（512 审阅收尾；现降为接手清单 P2）

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

# 1024 实机（借入机；设备号缺省 1，无需装 thirdparty）
bash run.sh -r npu -v Ascend910B4
RUN_WITH_MSPROF=1 bash run.sh -r npu -v Ascend910B4   # 非默认：要 msprof 报告时才加

# 1024 KEM golden 后端自检（非默认）
KEM_GOLDEN_BACKEND=python bash run.sh -r cpu -v Ascend910B4   # 强制回落
KEM_GOLDEN_CROSS=1        bash run.sh -r cpu -v Ascend910B4   # liboqs ≡ python 逐字节
```

会话结束前：刷新**本文件** + 当日 `qa/` + 动过的 `INDEX.md`。
