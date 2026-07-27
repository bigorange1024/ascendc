# STATUS — exp-fips203-mlkem-kem-decaps-ct-k2

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`** — ML-KEM-512 / `k=2` **CT 设备探针**（**PASS**，2026-07-27）。

| 项 | 值 |
|---|---|
| **阶段** | **PASS**（W3 / **D21ct**；CT path） |
| **I/O** | `dk_kem=1632B` + `c=768B` → `K=32B` |
| **dk 布局** | `dk_pke[0:768)` ‖ `ek[768:1568)` ‖ `h[1568:1600)` ‖ `z[1600:1632)` |
| **PKE** | Phase-D 编译期引用 D15 k2 Decrypt fused；Phase-E 编译期引用 D14 k2 Encrypt 几何 |
| **SIM 默认** | **`ASCENDC_SIM_HOST_MODE=decaps_2session`**（CT 锁定）；T19i 形态 **3 launch**：D15 fused → Phase-E prep → `l18_l19` pack+FO |
| **host fixture** | D19 k2 sibling 未落地；本探针默认用活跃 D15/D14 host oracle 生成匹配 `dk_pke/ek`，再本地拼 `H(ek)` 与 `z` |

## 验收（2026-07-27 / Cloud / Ascend910B4）

| 范围 | 命令 | 结果 |
|------|------|------|
| **accept CPU** | `bash run.sh -r cpu -v Ascend910B4` | `K.bin` **max_abs_diff=0** |
| **accept SIM** | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | `K.bin` **max_abs_diff=0**；Phase-D **163069** + Phase-E **407638** = Total **570707** tick |
| **reject CPU** | `KEM_DECAPS_REJECT=1 bash run.sh -r cpu -v Ascend910B4` | `K.bin` **max_abs_diff=0**；`K` vs `J(z‖c)` **0**；`K` vs accept-path K **197** |
| **reject SIM** | `KEM_DECAPS_REJECT=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | `REJECT PASS`；Phase-D **163025** + Phase-E **408344** = Total **571369** tick；`K` vs accept-path K **198** |
| **stray dump** | 用例根检查 | 无 `core*.dump` / `profile_*_log*.toml` / `*.dump` / `*.log` stray |

## 实现要点

- 行 1–4 只做 `dk_kem` 指针切片，不另开 launch。
- Phase-D 复用活跃 D15 k2 Decrypt fused；Phase-E 复用活跃 D14 k2 Encrypt 的 `du=10` / `dv=4` 布局，并在本探针本地 `l18_l19` 覆盖核尾部执行 FO。
- 本地 `l18_l19` 保持 k2 真实分片：AIV0 处理 `u0/u1`，AIV1 处理 `tr̂/v`，避免误沿用 k3 第三行。
- 本目录只改变 CT 编排默认（2-session）与 CT 状态口径；**禁止**改共享 D14/D15 树、从 `**/frozen/**` 抄实现，或回改 D21 delivery 探针的 `decaps_1session` 默认。
