# examples — 算子计算说明索引

**本 INDEX 的重点**：说明 `examples/` 下**每个子目录在算什么**（数学语义、数据类型与规模、工程角色）。运行方式见各目录 `run.sh`。

---

**自包含**（2026-06-29）：活跃 example 与探针同约束——除 `library/shared` 外不得跨目录引用源码；KeyGen 生产禁止 Host 辅助密码计算。见 [用例自包含与设备全链约束.md](../docs/engineering/用例自包含与设备全链约束.md)。

## 分层

| 路径 | 角色 |
|------|------|
| [incubating/](incubating/INDEX.md) | 研究中：`exp-<简述>/`，预研代码**只写这里** |
| [stable/](stable/INDEX.md) | 定型：`stable-<简述>-vN/`，从 `exp-*` **复制**晋级 |
| [frozen/](frozen/INDEX.md) | **路线关闭**：`frozen-exp-*`；只读 `FROZEN.md`/INDEX；**禁止抄码、禁止用 frozen customspec** |
| [../ascendc-tests/](../ascendc-tests/INDEX.md) | 平台功能探针；已关闭路线见 `ascendc-tests/frozen/`（同上） |

### 参数组嵌套（2026-07-26）

ML-KEM 活跃用例按参数组落在：

| 树 | 路径 |
|----|------|
| 探针 | `ascendc-tests/ml-kem/ml-kem-1024/` · **壳** `…/ml-kem-768/`（P0/P1） |
| 预研 | `examples/incubating/ml-kem/ml-kem-1024/` · **壳** `…/ml-kem-768/` |
| 定型 | `examples/stable/ml-kem/ml-kem-1024/`（**无** ml-kem-768） |

详表见各树 `INDEX.md`。**frozen 不迁入**参数组目录。768 参数卡：[`docs/specs/fips203-mlkem768-parameter-card.md`](../docs/specs/fips203-mlkem768-parameter-card.md)。

---

## 子目录与计算内容

### `incubating/`、`stable/`

见 [incubating/INDEX.md](incubating/INDEX.md)、[stable/INDEX.md](stable/INDEX.md) → 再进 `ml-kem/ml-kem-1024/INDEX.md`。

### `stable/ml-kem/ml-kem-1024/`（摘要）

| 目录 | 算什么 | 规模 / dtype | 角色 |
|------|--------|--------------|------|
| [stable-fips203-mlkem-pke-keygen-k4/](stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-keygen-k4/) | FIPS 203 **Alg.13 ML-KEM-1024 PKE KeyGen**（k=4）：`SEED_D`→`ek_PKE`/`dk_PKE` | 2 launch；SIM **542393** tick | **定型交付算子** |
| [stable-fips203-mlkem-pke-encrypt-k4/](stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/) | FIPS 203 **Alg.14 ML-KEM-1024 PKE Encrypt**（k=4）：`ek`+`m`+`coins`→**仅** `c` | SIM 2 launch；tick **627590** | **定型交付算子** |
| [stable-fips203-mlkem-pke-decrypt-k4/](stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4/) | FIPS 203 **Alg.15 ML-KEM-1024 PKE Decrypt**（k=4）：`dk`+`c`→**仅** `m` | 1 launch MIX；SIM **283290** tick | **定型交付算子** |
| [stable-fips203-mlkem-kem-keygen-k4/](stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4/) | FIPS 203 **Alg.19 ML-KEM-1024 KEM KeyGen**（k=4） | 2 launch；SIM tick **≈707k** | **定型交付算子** |
| [stable-fips203-mlkem-kem-encaps-k4/](stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/) | FIPS 203 **Alg.20 ML-KEM-1024 KEM Encaps**（k=4） | SIM 2 / CPU 5；tick **≈721k** | **定型交付算子** |
| [stable-fips203-mlkem-kem-decaps-k4/](stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4/) | FIPS 203 **Alg.21 ML-KEM-1024 KEM Decaps**（k=4） | **T19i SIM 3** / CPU 6；tick **1050620** | **定型交付算子**（`scripts/` 默认） |
| [stable-fips203-mlkem-kem-decaps-ct-k4/](stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-ct-k4/) | FIPS 203 **Alg.21 Decaps**（CT 专题副本） | SIM `decaps_2session` | **CT 实验副本**（非 scripts 默认） |

---

## 维护

新增 `exp-*` 或 `stable-*` → 更新对应参数组 `INDEX.md`，并视需要刷新本文件摘要表。
