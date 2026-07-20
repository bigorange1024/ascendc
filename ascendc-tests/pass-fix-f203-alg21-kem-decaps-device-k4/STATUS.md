# STATUS — pass-fix-f203-alg21-kem-decaps-device-k4

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`** — **无 vendor 设备主线**（**PASS**；自 `fix-…` 更名 2026-07-18）。

| 项 | 值 |
|---|---|
| **阶段** | **PASS**（2026-07-17 全链+E3+T2；2026-07-18 更名 `pass-fix`） |
| **历史 oracle** | **已冻结** — 只读 [`FROZEN.md`](../frozen/frozen-fix-f203-alg21-kem-decaps-correctness-k4/FROZEN.md)；**禁止**翻 frozen 源码 |
| **PKE** | 编译期引用 stable Decrypt fused + Encrypt |
| **笔记** | [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](../../docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |

## 验收

| 范围 | 命令 | 结果 |
|------|------|------|
| **全链 CPU** | `bash run.sh -r cpu …` | `K` **max=0** |
| **全链 SIM** | `bash run.sh -r sim …` | `K` **max=0**；单库 + 默认 `decaps_1session`；**T19i 后 3 launch**；D**287037**+E**763886** |
| **E3 拒绝** | `KEM_DECAPS_REJECT=1 …` | device `K` == liboqs == `J(z‖c)` **PASS** |
| liboqs 分项 KAT | `bash scripts/liboqs_kem_decaps_batch.sh` | **PASS** CPU×10+SIM×3（2026-07-17） |
| device roundtrip | `bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r cpu\|sim` | **PASS**（含拒绝） |

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

**实现要点**：行 1–4 指针偏移；SIM `prepare_dec_shim.sh` 合单库；**T19i PASS**：探针本地 `l18_l19` pack 尾同核 FO → SIM **3** launch（CPU 仍 6）；**禁止**改共享 PKE Encrypt 源。

**T19i 证据（2026-07-20）**：CPU/SIM `K` max=0；`KEM_DECAPS_REJECT=1` CPU+SIM PASS；SIM tick D**287037**+E**763886**；根无 stray dump。

**已外置**：`#交付#` → [`stable-…-kem-decaps-k4`](../../examples/stable/stable-fips203-mlkem-kem-decaps-k4/)（**T19i 已镜像**；SIM **3**）。
