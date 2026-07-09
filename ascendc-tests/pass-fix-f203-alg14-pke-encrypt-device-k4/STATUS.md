# STATUS — pass-fix-f203-alg14-pke-encrypt-device-k4

**前缀 `pass-`**：**CPU + SIM PASS**（2026-07-08 晋级）；`c.bin` max=0（两模式），SIM tick **626139**（≈方案估计 625k）。

**种子（已锁定）**：全链唯一种子 **`SEED_D=20260619`**；输入与 golden 复制自 [`fix-f203-alg14-pke-encrypt-correctness-k4`](../fix-f203-alg14-pke-encrypt-correctness-k4/)。单段探针各自种子（prep `20260706`、compute+tail `20260708`）**不**用于本全链。

**目标**：FIPS 203 Alg.14 **完整 K-PKE.Encrypt（行 1–22）** — `ek_pke` + `m` + `coins` → **仅** `c`（1568B，ML-KEM-1024）。

## 验收证据（2026-07-08）

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | `[cmp] c max=0` → `[SUCCESS] full encrypt: c matches golden` |
| SIM | `bash run.sh -r sim -v Ascend910B4` | `[cmp] c max=0`；Total tick **626139**；2 launch；根目录 **0 stray dump** |

**I/O 对齐 FIPS 203 Alg.14**：输入 `ek_pke`+`m`+`coins(r)`，输出 **仅密文 `c`（`output/c.bin`，1568B）**；u/v 为设备内部中间量，不 D2H、不落盘。
**覆盖**：FIPS 行 1–22 全在设备完成——行 1 `N←0`（无运算，隐含于 PRF nonce）、行 2 `t̂←ByteDecode₁₂(ek)`（在 `f203_encrypt_l18_l19` 核内 AIV0 解码，host 不传 t̂）、行 3–22（prep + compute+tail）。

**结论**：prep→compute a_hat/re **GM handoff 零拷贝语义正确**（无需转置，`a_hat_offset_jp(j,p)=(j*K+p)` 与 prep 存储 / correctness 三方一致）。

## 上游（均已 PASS）

| 探针 | 覆盖 | SIM tick |
|------|------|----------|
| [`pass-fix-f203-alg14-lines3-15-encrypt-prep-k4`](../pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/) | 行 3–15：ρ→`a_hat`；coins→`r‖e₁‖e₂` | **470502** |
| [`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/) | 行 2/16–24：compute + 内联 pack | **154781** |

## 目标 launch（定案）

| 模式 | launch 数 | 拓扑 |
|------|-----------|------|
| **SIM** | **2** | prep → `f203_encrypt_l18_l19`（含 tail pack） |
| **CPU** | **5** | prep + compute 三 launch + pack |

**预期 SIM tick**：~**625k**（470502 + 154781；不含 host 开销）。

## 验收原则（强制）

- **CPU+SIM 视为一件事**：两模式同一套输入都对拍 `c.bin` max=0 才算通过；不拆成独立里程碑。
- **锁死种子** `SEED_D=20260619`，输入与 golden **直接复用** [`fix-f203-alg14-pke-encrypt-correctness-k4`](../fix-f203-alg14-pke-encrypt-correctness-k4/)（该套已 device 验证 max=0）。
- golden 三源可互证：correctness `golden_c.bin` / 其 python `golden_c.py` / liboqs（`scripts/liboqs_pke_vs_ascendc.sh`）；对齐任一即 I/O 等价。仅用其 **I/O**，不抄实现。

## 文档

- 实现方案：[`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md)（§4.1 输入复用、§7 Gate、§8 Golden）

## 验收（复现，两条都要绿）

```bash
cd ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4   # 默认即 CAModel 金标；无需手动 SIM_DIRECT
```

- 对拍：`output/c.bin` vs `golden/c.bin`（**CPU ∧ SIM** max=0）；`output/` 仅密文 c（u/v 不落盘）
- `gen_data.py` **自包含**：优先复用 correctness 产物；缺失时本目录用 `SEED_D=20260619` 自生成 ek/m/coins/golden_c
- 默认 `ENCRYPT_SKIP_REBUILD=1`、`CMAKE_BUILD_JOBS=2`；强制重编：`ENCRYPT_FORCE_REBUILD=1`

## 禁止

- 从 [`fix-f203-alg14-pke-encrypt-correctness-k4`](../fix-f203-alg14-pke-encrypt-correctness-k4/) 抄码（旧 G5 拼装路线）
- 从 `ascendc-tests/frozen/` 带出实现
