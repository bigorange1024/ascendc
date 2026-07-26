# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-26（**768 W4 glue 已完成**；registry + exp roundtrip CPU/SIM 绿）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`** · PR [#12](https://github.com/bigorange1024/ascendc/pull/12) |
| **768 参数卡** | [`docs/specs/fips203-mlkem768-parameter-card.md`](docs/specs/fips203-mlkem768-parameter-card.md) **已锁**（§3.1–§3.3） |
| **探针 W0–W3** | **全绿**（CPU + `SIM_DIRECT=1` sim；tick 见 `qa/active_sim_regress_summary.md`） |
| **incubating-768** | **W4 + glue 已完成**：E13–E15 PKE exp 与 E19–E21ct KEM exp 均已 customspec + CPU/SIM；六份 registry 已补；`scripts/exp_kem768_liboqs_roundtrip.sh` AscendC-only CPU×1 + SIM×1 PASS |
| **stable-768** | **本阶段不建**（计划锁定） |

### 用户决议

1. T-B polyvec6 + Â 独立 prep；禁零垫  
2. 保留 D19–D21；要求 reject/CT（D21ct + E21ct）  
3. PKE exp 也要做（E13–E15）  
4. 命名 `-k3`  
5. **已授权** P2/W0–W3；**休息前一并授权 W4**（customspec + incubating 写码）

---

## ★ 剩余工作总表（授权范围）

### A. 768 主线 — 仍待做（本波授权）

| 优先级 | 项 | 说明 |
|--------|-----|------|
| **P0** | **W4a** E13–E15 | **已绿**（customspec → 从 D13–D15 晋级写码 → CPU+SIM） |
| **P0** | **W4b** E19–E21[+ct] | **已绿**（customspec → 从 D19–D21[+ct] 晋级 → CPU+SIM；E21ct reject≠accept 已验） |
| **P1** | registry 填绿 | **已完成**：六份 `docs/specs/fips203-mlkem768-*-baseline-registry.md` 按 E13–E21ct 绿线补登记 |
| **P1** | `scripts/exp_kem768_liboqs_roundtrip.sh` | **已完成**：当前 AscendC-only（liboqs helper 仍为 1024），CPU×1 + SIM×1 PASS，含 reject spot-check |
| **P2** | 可选 device/KAT 胶水 | D14↔D15 RT 脚本；各 device liboqs KAT / liboqs-768 helper（非门禁，有则做） |

### B. 明确不做（本阶段）

| 项 | 原因 |
|----|------|
| `examples/stable/ml-kem/ml-kem-768/**` | 计划锁定；须日后 `#交付#` |
| NPU 真机 | 无卡；另开 T2-npu |
| 从 frozen 抄码 / 零垫凑 4/8 | 仓规 |

### C. 仓级其它打开项（非 768 阻塞）

| ID | 事项 |
|----|------|
| T6f | KeyGen CPU flaky（隔离后未再现） |
| T23 | 多 AI Core 并行 stable |
| T21 | SHA3hp 可行性（待拍板） |
| 教材细表 | formal-lang 文档 |

---

## ★ 下一刀 — 可选后续

W4 incubating 与 glue 均已完成：**E13** `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-keygen-k3/` tick **373429**；**E14** `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-encrypt-k3/` tick **507633**；**E15** `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-decrypt-k3/` tick **222073**；**E19** `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-keygen-k3/` tick **510867**；**E20** `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-encaps-k3/` tick **590261**；**E21** `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-decaps-k3/` accept **820230** / reject **822500**；**E21ct** `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-decaps-ct-k3/` accept **826115** / reject **825836**。

本轮新增：

1. registry 填绿：`docs/specs/fips203-mlkem768-*-baseline-registry.md` 已补 E13–E21ct 已验证项；liboqs-768 / NPU 均仍标未跑。
2. 端到端：`scripts/exp_kem768_liboqs_roundtrip.sh` 默认无 `-ct`，当前为 AscendC-only；CPU `K` accept max=0、reject `J(z||c)` max=0；SIM `K` accept max=0、reject max=0。
3. 仍不建 `examples/stable/ml-kem/ml-kem-768/`，除非用户后续明确 `#交付#`。

可选下一刀：liboqs-768 helper / device KAT、D14↔D15 PKE RT、stable-768（需 `#交付#`）、非 768 打开项（T23 / T21 / T2-npu / 教材细表）。

---

## ★ Smoke

```bash
bash scripts/check_mlkem768_sizes.sh
test ! -d examples/stable/ml-kem/ml-kem-768
```

---

## ★ 勿做

- 从 `**/frozen/**` 抄实现；零垫凑 4/8  
- 无 customspec 写 incubating（本波：先写规格再写码）  
- 建 stable-768  
- 擅自改 §3.1–§3.3 已锁 tiling  
- 擅自改 `.cursor/rules/` / `.cursor/skills/`  
