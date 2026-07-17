# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · `docs/research/` 调研草稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-17（Decaps device 全链已推；**下一刀交 Cloud：T2 单库/单 session**）

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **KEM Encaps stable** | [`stable-fips203-mlkem-kem-encaps-k4`](examples/stable/stable-fips203-mlkem-kem-encaps-k4/) **定型**；tick **721119** |
| T19a Encaps device | [`pass-fix-…-encaps-device-k4`](ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/)；tick **721010** |
| **T19b/c Decaps device** | [`fix-…-decaps-device-k4`](ascendc-tests/fix-f203-alg21-kem-decaps-device-k4/) **全链 + E3 PASS**；CPU 单库；**SIM 双库 + 2-session 保底**；D**283317**+E**745341**；仓库 `scripts/` Decaps 默认已指 device |
| PKE + KEM KeyGen stable | 已交付 |

---

## ★ 下一刀（P0）— 交 **Cloud Agent**

### T2：SIM 单库合库 + 单 session 真修

**目标**（改工程折中，**不**改 Alg.18 语义）：

1. SIM 从 **双库**（`…_dec_sim` + `…_sim`）收敛为 **单** `libascendc_kernels_*.so`
2. 同一 ACL session 内连续 D→E（可先保留 `decaps_2session` 作对照；生产默认争取 `1session` 绿）

**阻塞点（已记录）**：

- stable Decrypt/Encrypt **同名头**（`aiv_func` / `ntt_vec` 等）→ ascendc precompile **无法 per-TU 隔离** → 当前被迫 SIM 双库
- 合库后仍须复验 **CAModel session 残留**（见 `docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`）

**必读**：

- [`INTEGRATION_PLAN.md`](ascendc-tests/fix-f203-alg21-kem-decaps-device-k4/INTEGRATION_PLAN.md) §单设备库 / §非目标
- [`cmake/decaps/CMakeLists.txt`](ascendc-tests/fix-f203-alg21-kem-decaps-device-k4/cmake/decaps/CMakeLists.txt)
- [`qa/2026-07/2026-07-17-Decaps-device-全链-PASS.md`](qa/2026-07/2026-07-17-Decaps-device-全链-PASS.md)

**验收**：同目录 `bash run.sh -r cpu|sim -v Ascend910B4`；全链 `K` max=0；E3 `KEM_DECAPS_REJECT=1` 仍绿。

**本机不做**：T2 编码交 Cloud；WSL 仅维护文档/推送基线。

次优先（可本机）：`pass-fix-…decaps-device-k4` 更名；KAT 扩量；更后 Decaps `#交付#`。

---

## ★ Smoke（当前基线，推送后）

```bash
cd ascendc-tests/fix-f203-alg21-kem-decaps-device-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4   # 默认 decaps_2session；勿再手写 SIM_DIRECT=1
```

---

## ★ 约束速记

- frozen：进门读判决书，出门不带码  
- examples 写码须活跃 `*-customspec.*`；ascendc-tests 不受此限  
- 已锁定参数不得擅自改；写 AscendC 前查 API 查阅索引  
- Decaps SIM 现状：**双库 + 2-session**；T2 目标单库/单 session  
