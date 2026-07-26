# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-26（**768 P2/W0 B1–B3 CPU+SIM 全绿**；下一刀 W1）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **分支** | **`research/formal-lang-dag`** · PR [#12](https://github.com/bigorange1024/ascendc/pull/12) |
| **768 参数卡** | [`docs/specs/fips203-mlkem768-parameter-card.md`](docs/specs/fips203-mlkem768-parameter-card.md) **已锁** |
| **768 P2/W0** | **B1/B2/B3 有条件完成**（CPU + `SIM_DIRECT=1` sim；根无 stray） |
| **W0 探针** | [`compress-decompress-du10-dv4-k3`](ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-compress-decompress-du10-dv4-k3/) · [`byteencode-decode-d-k3`](ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-byteencode-decode-d-k3/) · [`alg8-cbd-eta2-k3`](ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg8-cbd-eta2-k3/) |
| **incubating-768** | 仍为壳；**无** customspec 写码 |
| **stable-768** | **不建** |
| **1024** | 迁移已冒烟绿；encode12 golden 仍引用其 v2（k4 几何，d=12 算法探针） |

### 用户决议（参数卡）

1. 分核 **T-B**（polyvec6）+ Â 独立 prep  
2. 保留 KEM device D19–D21  
3. **要求** reject/CT（D21ct + E21ct）  
4. **PKE exp 也要做**  
5. 命名 `-k3`  
6. P0+P1 已完成；**已授权 P2/W0**（本波完成）

---

## ★ 下一刀（P0）— P2 / W1（须用户再授权）

**W1 积木**：B4 SampleNTT k3 · B5 polyvec6 NTT/INTT · B6 multiply/inner  

开写前：

- 探针：【预研】或等价授权  
- 补齐参数卡 §3 **数值 tiling**（polyvec6 / Â prep；遇阻停问）  
- incubating：仍须 `$规格$`（W4 才写 exp）  

---

## ★ Smoke（路径自检）

```bash
bash scripts/check_mlkem768_sizes.sh
cd ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg8-cbd-eta2-k3 && bash run.sh -r cpu -v Ascend910B4
test ! -d examples/stable/ml-kem/ml-kem-768
```

---

## ★ 勿做

- 从 `**/frozen/**` 抄实现；零垫凑 4/8  
- 无 customspec 写 `examples/incubating/ml-kem/ml-kem-768/**` 代码  
- 未压测绿建 `stable` / `ml-kem-768` stable  
- 擅自改 `.cursor/rules/` / `.cursor/skills/`  
