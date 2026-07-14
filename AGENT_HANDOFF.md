# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-14（办公室：runtime_env 扩到活跃探针；交 Cloud 跑测、本机不跑）

---

## ★ Cloud 下一刀（优先）

**任务**：`git pull` 最新 main 后，对 **ascendc-tests 活跃探针**（已接 `runtime_env`）跑 `cpu` + `sim`；**不要**跑两个 `NOT_IMPLEMENTED` device stub；**不要**跑 `frozen/`。

建议入口（可写成循环）：

```bash
git pull origin main && git rev-parse --short HEAD
# 冒烟短：
cd ascendc-tests/add_custom && bash run.sh cpu 910B4 && SIM_DIRECT=1 bash run.sh sim 910B4
cd ../pass-shake128-ops-math-toy && bash run.sh -r cpu -v Ascend910B4 && bash run.sh -r sim -v Ascend910B4
# 再按需扫 INDEX「当前探针」+ correctness（fix-alg19/20/21-*-correctness-k4）
```

说明：[`docs/notes/AscendC多环境运行纪要.md`](docs/notes/AscendC多环境运行纪要.md) · [`docs/engineering/NPU真机环境说明.md`](docs/engineering/NPU真机环境说明.md)。

已知陷阱：Cloud Clang + `-Werror` 会打未用常量（PKE keygen 已修）；SIM 符号错≠缺 liboqs。

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| `runtime_env` | 四例 examples 试点 + **活跃探针 run.sh** 已接入；默认仍 **cpu** |
| thirdparty | `clone-thirdparty.sh` 默认编 liboqs |
| Cloud SIM dump | 非 WSL 不装 dump 桩（`sim_env.sh`） |

---

## ★ 强制写法（2026-07-08）

| 类型 | 写法 | 禁止 |
|------|------|------|
| CPU vs SIM | `ASCENDC_BUILD_CPU` / `ASCENDC_BUILD_SIM` | per-probe 编译宏分叉 host |
| SIM host 拓扑 | `ASCENDC_SIM_HOST_MODE` + §3.3 登记表 | `F203_FEAS_*` 等 |
| 算法变体 | CMake 宏（CPU/SIM 同值） | env 切 launch |

**无 NPU 时**：PKE/KEM 全链 **SIM = 主参考**；CPU = 辅助。

---

## ★ 当前真相（算法侧，未变）

| 段 | 状态 |
|----|------|
| PKE 三段 stable | 齐备 |
| KEM Alg.19 KeyGen incubating | 有条件完成；**stable 须 `#交付#`** |
| 主线下一刀 | **T19a Encaps device**（待开工 stub） |

纪要：[`qa/2026-07/2026-07-14-…`](qa/2026-07/2026-07-14-KEM-KeyGen-incubating预研重写.md)

---

## ★ 办公室其它优先级

1. 等 Cloud 探针矩阵反馈（编译/SIM 分诊）  
2. 用户 `#交付#` → KEM KeyGen stable  
3. T19a Encaps device 开工  
