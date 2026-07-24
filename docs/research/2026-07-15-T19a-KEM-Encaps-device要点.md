# T19a — KEM Encaps device 调研要点（2026-07-15）

> **状态**：探针已 **CPU+SIM PASS**；本文为 research 侧摘要，权威实现方案见  
> [`ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/INTEGRATION_PLAN.md`](../../ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/INTEGRATION_PLAN.md)。  
> 定稿笔记（若需原理长文）日后再迁 `docs/notes/`。

## 锁定（相对 Alg.14 Encrypt 的增量）

| 项 | 约定 |
|----|------|
| 参数 | ml_kem_1024 / **k=4** |
| 代数 | Alg.17：`(K,r)←G(m‖H(ek))`，再 `Encrypt(ek,m,r)` |
| Encrypt 源 | **编译期引用** `stable-fips203-mlkem-pke-encrypt-k4`（禁止 frozen G5 vendor） |
| SHA3 头 | **并入** prep 入口前段；不另开 launch |
| `m` | GM 输入（`input/m.bin`）；`coins`/`r` 仅设备写 |
| Launch | = stable Encrypt（SIM 2 / CPU 5） |

## 验收（2026-07-15）

| 模式 | 结果 |
|------|------|
| CPU | `c`/`K` vs liboqs `encaps_derand` max=0 |
| SIM | 同上；Total tick **721010**（`SIM_DIRECT=1`） |

## 下一刀

- T19b/c：**交付** [`pass-fix-f203-alg21-kem-decaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/)（Phase-E / Phase-D）；**CT 专题** [`pass-fix-…-decaps-device-ct-k4`](../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-ct-k4/)
- 可选：本探针更名 `pass-fix-…`；仓库 `ENCAPS_DIR` / 分项 kat 改指 device
