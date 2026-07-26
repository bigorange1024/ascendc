# STATUS — exp-fips203-mlkem-kem-decaps-k3

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`** — ML-KEM-768 / `k=3` incubating 实现（E21）。

| 项 | 值 |
|---|---|
| **阶段** | **有条件完成**（W4b / E21；CPU + `SIM_DIRECT=1` sim 通过；delivery path，**非 `-ct`**；未跑 liboqs KAT / NPU） |
| **I/O** | `dk_kem=2400B` + `c=1088B` → `K=32B` |
| **PKE** | Phase-D 使用本目录 vendored E15/D15 k3 Decrypt fused；Phase-E 使用本目录 vendored E14/D14 k3 Encrypt 几何 |
| **SIM 默认** | `decaps_1session`；T19i 形态 **3 launch**：D15 fused → Phase-E prep → `l18_l19` pack+FO |
| **customspec** | [`exp-fips203-mlkem-kem-decaps-k3-实现方案-customspec.pdf`](exp-fips203-mlkem-kem-decaps-k3-实现方案-customspec.pdf) |
| **历史 oracle** | correctness 路线已冻结；本 exp 仅使用活跃 k3 基线迁入后的本地源码与本地 golden |

## 验收（2026-07-26 / Cloud / Ascend910B4）

| 范围 | 命令 | 结果 |
|------|------|------|
| **全链 CPU** | `bash run.sh -r cpu -v Ascend910B4` | `K.bin` **max_abs_diff=0** |
| **全链 SIM** | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | `K.bin` **max_abs_diff=0**；Phase-D **221059** + Phase-E **599171** = Total **820230** tick |
| **拒绝 CPU** | `KEM_DECAPS_REJECT=1 bash run.sh -r cpu -v Ascend910B4` | `K.bin` **max_abs_diff=0**；`K` vs `J(z‖c)` **0** |
| **拒绝 SIM** | `KEM_DECAPS_REJECT=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | `REJECT PASS`；Phase-D **220692** + Phase-E **601808** = Total **822500** tick |
| **stray dump** | 用例根检查 | 无 `core*.dump` / `profile_*_log*.toml` / `*.dump` stray |

## 实现要点

- 行 1–4 只做 `dk_kem` 指针切片：`dk_pke[0:1152)`、`ek[1152:2336)`、`h[2336:2368)`、`z[2368:2400)`，不另开 launch。
- Phase-D 使用 vendored D15 k3 Decrypt fused；Phase-E 使用 vendored D14 k3 Encrypt 的 `du=10` / `dv=4` 布局，并在本目录 `kem/f203_encrypt_l18_l19_kernel.cpp` 覆盖核尾部执行 FO。
- `scripts/gen_data.py` 不依赖 ML-KEM-1024 liboqs helper或其它用例；它用本目录 `scripts/keygen_golden.py` 与 `scripts/host_golden/` 生成合法 `c` 和 golden `K`。
- **禁止**改共享 E14/E15 树、从 `**/frozen/**` 抄实现，或把本 delivery exp 改成 `-ct` 两 session 默认。
