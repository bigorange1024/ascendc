# STATUS — pass-fix-f203-alg21-kem-decaps-device-k3

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`** — ML-KEM-768 / `k=3` 设备探针（**PASS**，2026-07-26）。

| 项 | 值 |
|---|---|
| **阶段** | **PASS**（W3 / D21；delivery path，**非 `-ct`**） |
| **I/O** | `dk_kem=2400B` + `c=1088B` → `K=32B` |
| **PKE** | Phase-D 编译期引用 D15 k3 Decrypt fused；Phase-E 编译期引用 D14 k3 Encrypt 几何 |
| **SIM 默认** | `decaps_1session`；T19i 形态 **3 launch**：D15 fused → Phase-E prep → `l18_l19` pack+FO |
| **历史 oracle** | correctness 路线已冻结；本探针仅使用活跃 k3 D14/D15/D19 fixture 与本地 golden |

## 验收（2026-07-26 / Cloud / Ascend910B4）

| 范围 | 命令 | 结果 |
|------|------|------|
| **全链 CPU** | `bash run.sh -r cpu -v Ascend910B4` | `K.bin` **max_abs_diff=0** |
| **全链 SIM** | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | `K.bin` **max_abs_diff=0**；Phase-D **220767** + Phase-E **597518** = Total **818285** tick |
| **拒绝 CPU/SIM** | `KEM_DECAPS_REJECT=1 ...` | `K.bin` **max_abs_diff=0**；`K` vs `J(z‖c)` **max_abs_diff=0** |
| **stray dump** | 用例根检查 | 无 `core*.dump` / `profile_*_log*.toml` / `*.dump` stray |

## 实现要点

- 行 1–4 只做 `dk_kem` 指针切片：`dk_pke[0:1152)`、`ek[1152:2336)`、`h[2336:2368)`、`z[2368:2400)`，不另开 launch。
- Phase-D 复用活跃 D15 k3 Decrypt fused；Phase-E 复用活跃 D14 k3 Encrypt 的 `du=10` / `dv=4` 布局，并在本探针本地 `l18_l19` 覆盖核尾部执行 FO。
- `scripts/gen_data.py` 不依赖 ML-KEM-1024 liboqs helper；它复用 D19 k3 keypair oracle 与 D14 k3 Encrypt host golden 生成合法 `c` 和 golden `K`。
- **禁止**改共享 D14/D15 树、从 `**/frozen/**` 抄实现，或把本 delivery 探针改成 `-ct` 两 session 默认。
