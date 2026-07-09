# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-10（PKE 三段 stable；KEM vendor **暂** frozen G5/G4；**T19** 待重构对齐 stable）

---

## ★ 强制写法（2026-07-08，新代码均须遵守）

| 类型 | 写法 | 禁止 |
|------|------|------|
| CPU vs SIM | `ASCENDC_BUILD_CPU` / `ASCENDC_BUILD_SIM` | per-probe 编译宏分叉 host |
| SIM host 拓扑 | `ASCENDC_SIM_HOST_MODE` + §3.3 登记表 | `F203_FEAS_*`、`KEM_DECAPS_SIM_2SESSION` 等 |
| 算法变体 | CMake 宏（CPU/SIM 同值） | env 切 launch |

详文：[`AscendC-CPU与SIM实现分叉开发指南.md`](docs/notes/AscendC-CPU与SIM实现分叉开发指南.md) §4.1 · [`library/shared/INDEX.md`](library/shared/INDEX.md)

**无 NPU 时验收权重**：全链 PKE — **SIM = 主参考**；**CPU = 辅助正确性**。

---

## ★ 当前真相（PKE 三段 stable 齐备，2026-07-10）

### Alg.15 Decrypt — **stable 交付** ★本轮完成

| 路径 | 角色 |
|------|------|
| [`examples/stable/stable-fips203-mlkem-pke-decrypt-k4`](examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) | **定型交付算子** |
| [`examples/incubating/exp-fips203-mlkem-pke-decrypt-k4`](examples/incubating/exp-fips203-mlkem-pke-decrypt-k4/) | 预研副本（保留） |
| [`ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4`](ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4/) | PASS 探针（对照） |

| 项 | 内容 |
|----|------|
| I/O | `dk_pke`+`c`+`lut_*` → **仅 `m` 32B** |
| Launch | **1×** `f203_decrypt_device_fused`（MIX `aicore=1`） |
| SIM | `m` max=0；tick **283290** |
| KAT | `kat_liboqs_vs_ascendc.sh` **CPU×10 + SIM×1** PASS（liboqs keygen + host golden_c fixture） |
| roundtrip | `roundtrip_pke_batch.sh` **CPU×10 + SIM×1** PASS |

纪要：[`qa/2026-07/2026-07-10-Decrypt交付stable.md`](qa/2026-07/2026-07-10-Decrypt交付stable.md)

### Alg.14 Encrypt — **stable 交付**

[`examples/stable/stable-fips203-mlkem-pke-encrypt-k4`](examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) — SIM tick ~627k；KAT×10+1 + roundtrip×10+1 PASS。

### Alg.13 KeyGen — **stable 交付**

[`examples/stable/stable-fips203-mlkem-pke-keygen-k4`](examples/stable/stable-fips203-mlkem-pke-keygen-k4/)

### 仓库闭环脚本默认

| 段 | 默认 |
|----|------|
| KeyGen | `examples/stable/stable-fips203-mlkem-pke-keygen-k4` |
| Encrypt | `examples/stable/stable-fips203-mlkem-pke-encrypt-k4` |
| Decrypt | `examples/stable/stable-fips203-mlkem-pke-decrypt-k4` |

---

## ★ 下一任务（P0）

1. **T19 — KEM 探针重构对齐 stable PKE**（见 `qa/TODO.md` T19a–e）  
   - 现状：alg20/21 `vendor_sync` ← frozen correctness（G5 / 2-launch G4）；stable Encrypt/Decrypt **布局不兼容** drop-in  
   - 目标：Encaps/Decaps vendor 吃 [`stable-…-encrypt/decrypt-k4`](examples/stable/)；完成后去掉 frozen 作 sync 源  
2. **Alg.21** 单 session SIM 真修（宜在 T19 后；`at_r5` / func_key）
3. **NPU 实机**；KEM Alg.19/20 **`#交付#`**（视产品节奏）

**禁止**：未改接线就把 `vendor_sync` SRC 指回 stable；从 frozen **抄码改写**冒充新实现（当前 rsync 拼装快照除外，直至 T19e）。

---

## 验收命令（smoke）

```bash
# ★ Decrypt stable（交付主线）
cd examples/stable/stable-fips203-mlkem-pke-decrypt-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh

# PKE 全链闭环（三段均 stable）
bash scripts/roundtrip_pke_batch.sh

# Encrypt stable（回归）
cd examples/stable/stable-fips203-mlkem-pke-encrypt-k4
bash run.sh -r sim -v Ascend910B4
```

**WSL**：`CMAKE_BUILD_JOBS=2`；勿并行多 SIM。

---

## 附录：关键路径速查

| 主题 | 路径 |
|------|------|
| **Decrypt stable** | [`examples/stable/stable-fips203-mlkem-pke-decrypt-k4/`](examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) |
| **Encrypt stable** | [`examples/stable/stable-fips203-mlkem-pke-encrypt-k4/`](examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) |
| **KeyGen stable** | [`examples/stable/stable-fips203-mlkem-pke-keygen-k4/`](examples/stable/stable-fips203-mlkem-pke-keygen-k4/) |
| 闭环脚本 | [`scripts/roundtrip_pke_batch.sh`](scripts/roundtrip_pke_batch.sh) |
| KAT fixture | [`scripts/liboqs_pke_decrypt_fixture.py`](scripts/liboqs_pke_decrypt_fixture.py) |
