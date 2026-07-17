# STATUS — fix-f203-alg21-kem-decaps-device-k4

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`** — **无 vendor 设备主线**（**T19b/c 全链 + E3 + T2 单库/1-session 已绿**）。

| 项 | 值 |
|---|---|
| **阶段** | **全链 + E3 + T2 PASS**（2026-07-17） |
| **后继自** | [`fix-f203-alg21-kem-decaps-correctness-k4`](../fix-f203-alg21-kem-decaps-correctness-k4/)（oracle；**禁止抄码**） |
| **PKE** | stable Decrypt fused + stable Encrypt；见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) |
| **笔记** | [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](../../docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |

## 验收（2026-07-17）

| 范围 | 命令 | 结果 |
|------|------|------|
| Phase-E-only | `KEM_DECAPS_PHASEE_ONLY=1 …` | E0–E2；tick **746221** |
| **全链 CPU** | `bash run.sh -r cpu …` | `K` **max=0** |
| **全链 SIM（T2）** | `bash run.sh -r sim …` | `K` **max=0**；**单库** + 默认 **`decaps_1session`**；D**286803**+E**745925** |
| **E3 拒绝** | `KEM_DECAPS_REJECT=1 bash run.sh -r cpu\|sim …` | 假密文；device `K` == liboqs Decaps == `J(z‖c)` **PASS**（含 1-session SIM） |

**说明**：liboqs Decaps **不暴露内部 c'**；E3 对拍对象是最终 `K`（Alg.18 行 9–11 隐式拒绝输出）。

**实现要点（T2）**：

- 行 1–4 指针偏移；CPU/SIM **均为单** `libascendc_kernels_*.so`
- SIM：`scripts/prepare_dec_shim.sh` 自 stable Decrypt 生成 `shim/pke_decrypt/`（冲突头 → `dec_*`），合进一个 `ascendc_library`
- 默认 `ASCENDC_SIM_HOST_MODE=decaps_1session`；`decaps_2session` 仅对照

**已做**：仓库级 Decaps scripts 默认指本目录；**T2 单库 + 1-session**。

**未做**：`pass-fix` 更名；KAT 扩量；`#交付#`。
