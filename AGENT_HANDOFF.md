# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-08-03（**订正**：同卡污染 ≠ device1 坏卡；`DeviceGuard` + KeyGen LUT 硬失败 + `F203_L18_TRACE`）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`main`**（本轮 ACL/LUT/trace 已推；先 `git pull`）。  
2. Cloud 首次：`bash scripts/clone-thirdparty.sh`（含 liboqs；须保留完整 build 树以便编 PKE ref）。  
   —— 注意：**1024 的 14 个用例现在没有 liboqs 也能跑**（KEM golden 会回落 python），但**有 liboqs 才有权威交叉**，Cloud 仍建议装。  
3. **下一任务方向**：见下「★ 接手清单」与 [`qa/2026-08/2026-08-03-实机device1-l18复跑死锁.md`](qa/2026-08/2026-08-03-实机device1-l18复跑死锁.md) **§4**。用户口头指定优先级时以用户为准。  
4. 先读本文件 → [`qa/TODO.md`](qa/TODO.md) → 上条当日纪要。  
5. 写 AscendC 前：Rule + [`ascendc-engineering-notes`](.cursor/skills/ascendc-engineering-notes/SKILL.md)（含 §8.1 排程）。

### 刚关闭

| 项 | 说明 |
|----|------|
| **T18** | PKE helper encrypt/decrypt 链接：链 `ml_kem_1024_ref` `.o` + fips202 shim + `liboqs-internal.a`；**非改名问题** |

### 待办快照（新增，非本阶段主线）

| 项 | 说明 |
|----|------|
| **T2-npu-env** | **1024 全覆盖**：探针×7 + stable×7 走 `${REPO_ROOT}/scripts/env.sh`；npu `ASCEND_DEVICE_ID` 缺省 **0**（常规默认）；SIM 强制 0；`msprof_run.sh` 默认不采集 |
| **同卡污染（08-03 订正）** | **不是**「device1 坏卡」。探针挂死/Ctrl+C/`timeout` 未 Finalize → **同卡**连环挂；换卡只绕开。已加 [`acl_session::DeviceGuard`](library/shared/acl_session/acl_session.hpp) |
| **alg19 错结果** | NPU 路径曾**静默吞 LUT ReadFile 失败**（已改硬失败）；实机须 `FORCE_REBUILD` 复验是否仍 max≠0 |
| **alg20/21 首次卡 `l18_l19`** | 主因未关；实机 `F203_L18_TRACE=1` 看 fused-trace 槽位 |
| **实机 msprof** | `RUN_WITH_MSPROF=1` 才采集；见 [`scripts/msprof_run.sh`](scripts/msprof_run.sh) |
| **零 thirdparty golden** | LUT 三级回退；[`f203_kem_ref`](library/shared/f203_kem_ref/kem_ref.py) |

---

## ★ 接手清单（2026-08-03 订正后）

| 优先 | 项 | 做什么 | 注意 |
|------|----|--------|------|
| **P0** | 探针主因 | 干净卡上**单跑** alg19（看 max / LUT 日志）→ 再单跑 alg20 `F203_L18_TRACE=1` | **勿**在已污染卡上排主因；杀进程后 `npu-smi` Process |
| **P0** | 提交/推送 | 仅当用户明确要求：DeviceGuard + LUT 检查 + L18 trace + 文档订正 | 无授权不 commit/push |
| **P1** | **T2-npu-link** | 512 / 768 / incubating 剩 12 处 `ln -sfn` → `ln -sfnr` | 改哪档验哪档 |
| **P2** | 512 审阅收尾 | 见下节 | 与 NPU 线互不冲突 |

**别做**：把「device1 坏」写回文档；`#交付#` stable-512/768（须用户点名）；从 frozen 抄码。

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

# 1024 实机（借入机；设备号缺省 0，无需装 thirdparty）
bash run.sh -r npu -v Ascend910B4
# 非默认：物理 0 被占用时才 ASCEND_DEVICE_ID=1（该卡曾复跑死锁）
RUN_WITH_MSPROF=1 bash run.sh -r npu -v Ascend910B4   # 非默认：要 msprof 报告时才加

# 1024 KEM golden 后端自检（非默认）
KEM_GOLDEN_BACKEND=python bash run.sh -r cpu -v Ascend910B4   # 强制回落
KEM_GOLDEN_CROSS=1        bash run.sh -r cpu -v Ascend910B4   # liboqs ≡ python 逐字节
```

会话结束前：刷新**本文件** + 当日 `qa/` + 动过的 `INDEX.md`。
