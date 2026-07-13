# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-13（KEM KeyGen exp **规格-only 重置**；实现树删除；回家按 customspec 重写）

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

## ★ 当前真相（2026-07-13）

### PKE 三段 — **stable 交付齐备**（未改）

| 段 | stable | SIM tick（参考） |
|----|--------|------------------|
| KeyGen | [`stable-fips203-mlkem-pke-keygen-k4`](examples/stable/stable-fips203-mlkem-pke-keygen-k4/) | ~542k |
| Encrypt | [`stable-fips203-mlkem-pke-encrypt-k4`](examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) | ~627k |
| Decrypt | [`stable-fips203-mlkem-pke-decrypt-k4`](examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) | ~283k |

### KEM Alg.19 KeyGen — **设备探针 PASS；exp 实现已清空**

| 路径 | 角色 |
|------|------|
| [`pass-fix-f203-alg19-kem-keygen-device-k4`](ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/) | **行为/I/O 基线**（2 launch；~713k）；**可读行为，禁止当 CMake 依赖抄码晋级** |
| [`exp-fips203-mlkem-kem-keygen-k4/`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/) | **仅** [`…-实现方案-customspec.tex/.pdf`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf)（含 §踩坑 SyncAll）；**无源码** |
| `examples/stable/stable-fips203-mlkem-kem-keygen-k4/` | **已删除**；待 incubating 重写并验收后再 `#交付#` 复制晋级 |
| registry | [`docs/specs/fips203-mlkem1024-kem-keygen-baseline-registry.md`](docs/specs/fips203-mlkem1024-kem-keygen-baseline-registry.md) |

**办公室 debug 结论（回家必读 customspec §landmines + 当日 qa）**：

1. 过早晋级 stable 被用户否决 → 正确流程：incubating 压测绿 → 再 `#验收#`。
2. 偶发 `ek` PASS / `dk` FAIL：坏在 `dk_pke[1152:)`（AIV1 末 poly）；根因是 **AIV0 Fuse/Tail 抢跑**，本核 `PipeBarrier` **不能**汇合双 AIV。
3. 强制：SIM/设备 Encode 后 `SyncAll<isAIVOnly=true>()` 再 AIV0 做尾；CPU 由 `subBlockID==1` 做尾；`KYBER_PIPE_ALL` 禁空操作；CPU×N（清零 output）+ SIM 才可声称通过。
4. 用户裁决：删掉 stable + incubating **全部实现**，只留强化后的 customspec，回家 Agent **【预研】从零写**。

纪要：[`qa/2026-07/2026-07-13-thirdparty外部仓清单.md`](qa/2026-07/2026-07-13-thirdparty外部仓清单.md) §7–§10

---

## ★ 下一任务（P0）— 家里 Agent

**【预研】按 customspec 重写** [`exp-fips203-mlkem-kem-keygen-k4`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/)

1. 先读：上述 PDF（全文，尤其 **踩坑 / SyncAll**）+ registry + device 探针 **STATUS/行为**（不抄进 CMake）。
2. 自 `stable-fips203-mlkem-pke-keygen-k4` **一次性 vendor** PKE，加 `kem/` 尾与 `F203_KEM_KEYGEN_TAIL=1`；遵守双 AIV 汇合条款。
3. 验收：`bash run.sh -r cpu` + CPU 多轮压测 + `SIM_DIRECT=1 bash run.sh -r sim`；**勿**建 stable，除非用户 `#交付#`。

并行遗留（可稍后）：**T19a** Encaps device；**T21** SHA3hp 分析。

**禁止**：从 frozen / 已删树备份 / 对话残留 **抄实现**；未绿晋级 stable。

---

## 验收命令（smoke）

```bash
# PKE 全链（三段 stable）
bash scripts/roundtrip_pke_batch.sh

# KEM KeyGen 行为基线（探针；非 exp）
cd ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

**WSL**：`CMAKE_BUILD_JOBS=2`；勿并行多 SIM。

---

## 附录：关键路径速查

| 主题 | 路径 |
|------|------|
| KEM KeyGen **规格（待重写）** | [`exp-…-kem-keygen-k4-实现方案-customspec.pdf`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf) |
| KEM KeyGen device 基线 | [`pass-fix-f203-alg19-kem-keygen-device-k4/`](ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/) |
| 当日 debug 纪要 | [`qa/2026-07/2026-07-13-….md`](qa/2026-07/2026-07-13-thirdparty外部仓清单.md) |
| 遗留总表 | [`qa/TODO.md`](qa/TODO.md) |
