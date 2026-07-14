# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-14（办公室：`ntt_onnx` 公开；Cloud 先复验 clone）

---

## ★ Cloud 下一刀（优先）

1. **立刻**：`FORCE=1 ONLY=ntt_onnx BUILD_LIBOQS=0 bash scripts/clone-thirdparty.sh`（仓已公开；须见到 `transpose_mlkem_luts_i8.h`）
2. 再重跑原先因缺 LUT 挂的 Encrypt 探针（`alg14-*-encrypt-*`）cpu+sim
3. 其余失败（Clang `-Werror` / rsync / 仓内破损）**先别一起改**——等用户下一口令

说明：[`docs/notes/AscendC多环境运行纪要.md`](docs/notes/AscendC多环境运行纪要.md) · [`docs/engineering/thirdparty-本地依赖.md`](docs/engineering/thirdparty-本地依赖.md)

---

## ★ 多环境 / 依赖

| 项 | 说明 |
|----|------|
| `runtime_env` | 四例 examples 试点 + **活跃探针 run.sh** 已接入；默认仍 **cpu** |
| thirdparty | `clone-thirdparty.sh` 默认编 liboqs；**`ntt_onnx` 已公开**（HTTPS 匿名可拉） |
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
