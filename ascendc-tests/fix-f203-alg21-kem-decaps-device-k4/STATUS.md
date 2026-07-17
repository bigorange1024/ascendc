# STATUS — fix-f203-alg21-kem-decaps-device-k4

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`** — **无 vendor 设备主线**（**T19b/c 全链 + E3 拒绝已绿**）。

| 项 | 值 |
|---|---|
| **阶段** | **全链 + E3 PASS**（2026-07-17） |
| **后继自** | [`fix-f203-alg21-kem-decaps-correctness-k4`](../fix-f203-alg21-kem-decaps-correctness-k4/)（oracle；**禁止抄码**） |
| **PKE** | stable Decrypt fused + stable Encrypt；见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) |
| **笔记** | [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](../../docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |

## 验收（2026-07-17）

| 范围 | 命令 | 结果 |
|------|------|------|
| Phase-E-only | `KEM_DECAPS_PHASEE_ONLY=1 …` | E0–E2；tick **746221** |
| **全链 CPU** | `bash run.sh -r cpu …` | `K` **max=0** |
| **全链 SIM** | `bash run.sh -r sim …` | `K` **max=0**；D**283317**+E**745341** |
| **E3 拒绝** | `KEM_DECAPS_REJECT=1 bash run.sh -r cpu\|sim …` | 假密文；device `K` == liboqs Decaps == `J(z‖c)` **PASS** |

**说明**：liboqs Decaps **不暴露内部 c'**；E3 对拍对象是最终 `K`（Alg.18 行 9–11 隐式拒绝输出）。

**实现要点**：行 1–4 指针偏移；CPU 单库；SIM 双库 + 2-session。

**已做**：仓库级 `scripts/roundtrip_kem_decaps.sh`、`roundtrip_kem_keygen_encaps_decaps.sh`、`liboqs_kem_vs_ascendc.sh`、`kat_liboqs_kem_decaps.py` 默认改指 device；CPU KAT×1 PASS。

**未做**：`pass-fix` 更名；**SIM 单库合库 / 1-session（T2 → 交 Cloud Agent）**；`#交付#`。
