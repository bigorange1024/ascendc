# STATUS — fix-f203-alg14-pke-encrypt-correctness-k4

**定位**：Alg.14 **设备 AscendC 拼装**正确性探针（见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md)、[`SELF_CONTAINED.md`](SELF_CONTAINED.md)）。

**阶段**：**G5 双模式 PASS — SIM 全 device 单 session**（2026-06-30 晚：at_r5 + MIX 占位 G4 + 旧 G3 四核冻结）。

---

## SIM 测试通过声明（2026-06-30）

**结论：本探针 SIM 路径首次完整测试通过。**

| 项 | 内容 |
|----|------|
| 日期 / 时间 | 2026-06-30 13:58（UTC+8，**全 device G4** 复验） |
| 命令 | `bash run.sh -r sim -v Ascend910B4`（默认含 c.bin 对拍） |
| 工作目录 | 本目录（`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/`）|
| `bash` 退出码 | **0** |
| `ascendc_kernels_bbit` 内 `aclrtLaunchKernel` 返回 `507000` 次数 | **0** |
| `[verify_gate]` G1 / G2 / G3 | 全 PASS（max=0；`a_hat 4096` / `r 1024` / `e1 1024` / `e2 256` / `r_hat 1024` / `u_hat 1024` / `tr_hat 256` 系数） |
| `[verify] PASS max=0 (1568 bytes)` | ✅（c.bin 与 golden_c.bin 字节级一致） |
| 最终日志行 | `[SUCCESS] fix-f203-alg14-pke-encrypt-correctness-k4 gate=G5 (sim) ENCRYPT_VERIFY=1` |
| `Total tick`（CAModel，prep..pack 全 device）| **922441** |
| `Model RUN TIME`（CAModel）| 484906 ms |
| `wall_sec`（含 build + verify）| 485.59 s |
| SIM `device_aiv.o` AIV-only 核数 | **5**（marker / prep_a_hat / prep_re / g4_noise / at_r5；decode / pack 为 MIX） |
| host binary `nm out/bin/ascendc_kernels_bbit \| grep -E "f203_encrypt_(g3_linear\|at_r[^5]\|t_dot_r)"` | 空（无残留死引用） |

**对照旧 SIM 路径**（同探针、2026-06-30 早 at_r5 落地前）：`[g5_sim] at_r5 launch ret=507000` / `timeout: the monitored command dumped core` / `c.bin` 未写出。

**CPU 孪生**（同日同命令 `-r cpu`）：退出码 0、`[verify] PASS max=0 (1568 bytes)`、`Total tick` n/a（CPU 模式无 CAModel）、`wall_sec` ~11s。

> 此段为本探针 SIM 测试通过的**第一证据**。任何关于「SIM 是否测通」的问询直接引用本段。
> 详细排查史与原理沉淀见 [`G3_SIM_AUDIT.md`](G3_SIM_AUDIT.md) §12 / [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) §9 / [`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](../../docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md)。

**审计**：[`G3_SIM_AUDIT.md`](G3_SIM_AUDIT.md)（§1–§11 历史错误判读、**§12 病根修正**）。  
**原理沉淀**：[`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](../../docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md)。  
**纪要**：[`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](../../qa/2026-06/2026-06-30-funckey-507000本地独立验证.md)（§9 排查回顾、§10 性能、§11 经验）。

## 验收命令

```bash
# 默认 = gate + c.bin 对拍（生产验收）
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

## 结果（2026-06-30）

| 验收项 | CPU | SIM |
|---|---|---|
| `gate_g1/g2/g3`（`verify_gate.py`） | max=0 | max=0 |
| `ENCRYPT_VERIFY=1` c.bin vs golden_c.bin | **max=0** (1568B) | **max=0** (1568B) |
| `wall_sec` | ~11s | ~486s |
| `Total tick`（CAModel，prep..pack 全 device）| n/a | **922441** |
| `aclrtLaunchKernel` 返回 `507000` | n/a | **无** |
| host binary 残留 `g3_linear/at_r/t_dot_r` 引用 | n/a | 空 |
| SIM `device_aiv.o` AIV-only 核数 | n/a | **5** |

## G5 实现要点（at_r5 落地后）

| 段 | CPU | SIM |
|---|----|----|
| G1 prep（`prep_a_hat` + `prep_re`） | device kernel | device kernel |
| G2 NTT(r̂) | device kernel | device kernel |
| `decode_t_hat`（ek → t̂） | AIV_ONLY | **MIX 占位** |
| **G3 线性层（û + tr̂）** | **`at_r5` 合并核**（kP=5）| **`at_r5` 合并核** |
| G4 INTT u/tr | device MIX | device MIX |
| G4 噪声 + pack | device AIV | device（**g4_noise** AIV + **pack** MIX 占位）|

**单 ACL session**：CPU 走 `run_encrypt_g5_cpu_full`、SIM 走 **`run_g5_sim_full`**（prep..pack 全 device，一次 `aclInit/aclFinalize`）。

## funckey 守卫（永久保留）

`F203_FUNCKEY_EXPERIMENT` CMake 缓存变量（默认 `0`）：

- `=0`（生产默认）：AIV-only 5 核 + MIX decode/pack/ntt/intt，0 影响。
- `=1`：从 `KERNEL_FILES` 移除 `at_r5` + `pack`，缩 AIV 核做 funckey 边界实验（详 `qa/2026-06/2026-06-30` §3）。

## 已冻结 — Gate 过渡路线（G0–G4）

**G5** = 唯一生产验收。G0–G4 每测通下一 Gate 即冻结上一 Gate；G5 PASS 后全部关闭。

判决书：[`frozen-gates/FROZEN.md`](frozen-gates/FROZEN.md) · 索引：[`frozen-gates/INDEX.md`](frozen-gates/INDEX.md)

## 已冻结 — 旧 G3 拆分核（禁止参编）

| 目录 | kernel | 状态 |
|------|--------|------|
| [`compute/frozen/frozen-g3_linear/`](compute/frozen/frozen-g3_linear/) | g3_linear / g3_linear4 / at_r / t_dot_r | **冻结** |
| [`compute/frozen/frozen-at_r/`](compute/frozen/frozen-at_r/) | 独立 at_r 副本 | **冻结** |
| [`compute/frozen/frozen-t_dot_r/`](compute/frozen/frozen-t_dot_r/) | t_dot_r | **冻结** |

判决书：[`compute/frozen/FROZEN.md`](compute/frozen/FROZEN.md)。继任：[`compute/at_r5/`](compute/at_r5/)。

## 遗留

无阻塞项。G0–G4 过渡路线已标准化冻结 → [`frozen-gates/FROZEN.md`](frozen-gates/FROZEN.md)；`main_encrypt.cpp` 内 gate&lt;5 分支仅历史回放（启动 WARN）。

## 跨探针 round-trip（补充验收）

单探针默认 `ENCRYPT_VERIFY=1` 对拍 host `golden_c.bin`。device **c → Decrypt → m** 闭环见 [`scripts/roundtrip_pke_encrypt_decrypt.sh`](../../scripts/roundtrip_pke_encrypt_decrypt.sh)（KeyGen 密钥 + `SEED_D=20260619`）；CPU+SIM **max=0**（2026-06-30，[`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](../../qa/2026-06/2026-06-30-funckey-507000本地独立验证.md) §16）。
