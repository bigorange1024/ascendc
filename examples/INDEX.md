# examples — 算子计算说明索引

**本 INDEX 的重点**：说明 `examples/` 下**每个子目录在算什么**（数学语义、数据类型与规模、工程角色）。运行方式见各目录 `RUN.md` 或 `run.sh`。

---

**自包含**（2026-06-29）：活跃 example 与探针同约束——除 `library/shared` 外不得跨目录引用源码；KeyGen 生产禁止 Host 辅助密码计算。见 [用例自包含与设备全链约束.md](../docs/engineering/用例自包含与设备全链约束.md)。

## 分层

| 路径 | 角色 |
|------|------|
| [incubating/](incubating/INDEX.md) | 研究中：`exp-<简述>/`，预研代码**只写这里** |
| [stable/](stable/INDEX.md) | 定型：`stable-<简述>-vN/`，从 `exp-*` **复制**晋级 |
| [frozen/](frozen/INDEX.md) | **路线关闭**：`frozen-exp-*`；只读 `FROZEN.md`/INDEX；**禁止抄码、禁止用 frozen customspec** |
| [../ascendc-tests/](../ascendc-tests/INDEX.md) | 平台功能探针；已关闭路线见 `ascendc-tests/frozen/`（同上） |

---

## 子目录与计算内容

### `incubating/`、`stable/`

见 [incubating/INDEX.md](incubating/INDEX.md)、[stable/INDEX.md](stable/INDEX.md)。
### `stable/`

| 目录 | 算什么 | 规模 / dtype | 角色 |
|------|--------|--------------|------|
| [stable-fips203-mlkem-pke-keygen-k4/](stable/stable-fips203-mlkem-pke-keygen-k4/) | FIPS 203 **Alg.13 ML-KEM-768 PKE KeyGen**（k=4）：`SEED_D`→`ek_PKE`/`dk_PKE` | 2 launch；SIM **542393** tick（CPU/SIM/KAT ✓） | **定型交付算子** |
| [stable-fips203-mlkem-pke-encrypt-k4/](stable/stable-fips203-mlkem-pke-encrypt-k4/) | FIPS 203 **Alg.14 ML-KEM-1024 PKE Encrypt**（k=4）：`ek`+`m`+`coins`→**仅** `c` | SIM 2 launch；**SIM 主参考** tick **627590**（CPU 辅助；KAT×10+1/roundtrip×10+1 ✓） | **定型交付算子** |
| [stable-fips203-mlkem-pke-decrypt-k4/](stable/stable-fips203-mlkem-pke-decrypt-k4/) | FIPS 203 **Alg.15 ML-KEM-1024 PKE Decrypt**（k=4）：`dk`+`c`→**仅** `m` | 1 launch MIX；SIM **283290** tick（CPU/SIM/KAT×10+1/roundtrip×10+1 ✓） | **定型交付算子** |
| [stable-fips203-mlkem-kem-keygen-k4/](stable/stable-fips203-mlkem-kem-keygen-k4/) | FIPS 203 **Alg.19 ML-KEM-1024 KEM KeyGen**（k=4）：`SEED_D`→`ek_kem`/`dk_kem` | 2 launch；SIM tick **≈707k**（CPU/SIM/liboqs KAT ✓） | **定型交付算子** |
| [stable-fips203-mlkem-kem-encaps-k4/](stable/stable-fips203-mlkem-kem-encaps-k4/) | FIPS 203 **Alg.20 ML-KEM-1024 KEM Encaps**（k=4）：`ek`+`m`→**仅** `c`/`K`（设备 H/G） | SIM 2 / CPU 5；SIM tick **≈721k**（CPU/SIM/liboqs KAT ✓） | **定型交付算子** |




---

## 维护

新增 `exp-*` 或 `stable-*` → 更新 `incubating/` 或 `stable/` 的 `INDEX.md`，并在本文件增加一节（计算一句话、dtype、规模、角色）。
