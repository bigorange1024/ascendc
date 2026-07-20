# STATUS — pass-fix-f203-alg21-kem-decaps-device-k4

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`** — **无 vendor 设备主线**（**PASS**；自 `fix-…` 更名 2026-07-18）。

| 项 | 值 |
|---|---|
| **阶段** | **PASS**（2026-07-17 全链+E3+T2；2026-07-18 更名 `pass-fix`） |
| **对照 oracle** | [`fix-f203-alg21-kem-decaps-correctness-k4`](../fix-f203-alg21-kem-decaps-correctness-k4/)（**禁止抄码**） |
| **PKE** | 编译期引用 stable Decrypt fused + Encrypt |
| **笔记** | [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](../../docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |

## 验收

| 范围 | 命令 | 结果 |
|------|------|------|
| **全链 CPU** | `bash run.sh -r cpu …` | `K` **max=0** |
| **全链 SIM** | `bash run.sh -r sim …` | `K` **max=0**；单库 + 默认 `decaps_1session`；D**286803**+E**745925** |
| **E3 拒绝** | `KEM_DECAPS_REJECT=1 …` | device `K` == liboqs == `J(z‖c)` **PASS** |
| liboqs 分项 KAT | `bash scripts/liboqs_kem_decaps_batch.sh` | **PASS** CPU×10+SIM×3（2026-07-17） |
| device roundtrip | `bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r cpu\|sim` | **PASS**（含拒绝） |

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

**实现要点**：行 1–4 指针偏移；SIM `prepare_dec_shim.sh`（冲突头 `dec_*`）合单库；SIM 4 launch（含过渡 `fo_only`）；CPU 6 launch。

**未做（非 pass 门禁）**：`fo_only` 收回 `l18_l19` 尾；`#交付#` → stable。
