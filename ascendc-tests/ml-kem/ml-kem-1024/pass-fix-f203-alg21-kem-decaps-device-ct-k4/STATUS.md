# STATUS — pass-fix-f203-alg21-kem-decaps-device-ct-k4

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`** — **无 vendor 设备主线**（T19b/c；教材第7章 CT → 实现）。

| 项 | 值 |
|---|---|
| **阶段** | **PASS**（2026-07-24：本分支按第7章前瞻 CT 接线并验收） |
| **CT** | [`docs/research/…教材草案.tex`](../../docs/research/从已验证能力到合法派生-面向Agent预研的形式方法教材草案.tex) §前瞻 `CT_decaps`（先于本目录写码提交） |
| **PKE** | **编译期引用** [`stable-…-decrypt-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4/) fused + [`stable-…-encrypt-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/) |
| **KEM** | `kem/`：`G(m'‖h)` 并入 Phase-E prep；设备 FO（SIM：`l18_l19` 同核；CPU：`pack_fo`） |
| **SIM host** | 生产默认 **`ASCENDC_SIM_HOST_MODE=decaps_2session`**（CT 锁定）；单库合库（`prepare_dec_shim`） |
| **对照 oracle** | [`fix-…-decaps-correctness-k4`](../fix-f203-alg21-kem-decaps-correctness-k4/) — **只读 STATUS/计划**；**禁止**抄 `.cpp/.hpp` / vendor |
| **笔记** | [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](../../docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |

## 验收（2026-07-24 Cloud）

| 范围 | 命令 | 结果 |
|------|------|------|
| **全链 CPU** | `bash run.sh -r cpu -v Ascend910B4` | `K` **max=0** **PASS** |
| **全链 SIM** | `SIM_DIRECT=1 bash run.sh -r sim …` | `K` **max=0** **PASS**；D tick **286798** + E **763663**；根无 stray dump；单 `libascendc_kernels_sim.so` |
| **拒绝 CPU** | `KEM_DECAPS_REJECT=1`（=`TAMPER_C` 别名） | device `K` == liboqs == `J(z‖c)` **PASS** |
| **拒绝 SIM** | `KEM_DECAPS_REJECT=1 SIM_DIRECT=1 …` | `REJECT PASS`；D≈**286703** + E≈**763747**；根无 stray |

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-ct-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## 相对 correctness 的结构差异

| 轴 | correctness | 本探针 |
|----|-------------|--------|
| PKE 源 | frozen G4/G5 `vendor/` | **stable** 编译期引用 + SIM `prepare_dec_shim`（不改 stable） |
| Launch | 多段 G4+G5 | Decrypt **1** fused + Encrypt 同 stable；KEM 头/尾嵌入 |
| FO | 独立/嵌入 vendor pack | 探针本地 `kem/` + `l18_l19` 覆盖（勿改共享 Encrypt） |
| SIM | 默认 2-session | **同**（CT 生产默认） |
| vendor 树 | 有 | **无** |

## 交付落点（2026-07-24）

本探针仍为**行为基线**（禁止作 CMake 依赖）。定型交付：[`examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-ct-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-ct-k4/)（`#交付#` 自 incubating 复制）。
