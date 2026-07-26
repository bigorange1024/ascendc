# INTEGRATION_PLAN — pass-fix-f203-alg20-kem-encaps-device-k3

**定位**：`ascendc-tests/` **ML-KEM-768 Alg.20 `ML-KEM.Encaps` 设备探针**。本目录从活跃 k4 Encaps 编排复制而来，接入活跃 D14 k3 Encrypt 几何。

**活跃来源**：

| 路径 | 角色 |
|------|------|
| [`../pass-fix-f203-alg14-pke-encrypt-device-k3/`](../pass-fix-f203-alg14-pke-encrypt-device-k3/) | D14 PKE Encrypt k3 绿树：prep、compute、tail、host golden |
| [`../pass-fix-f203-alg19-kem-keygen-device-k3/`](../pass-fix-f203-alg19-kem-keygen-device-k3/) | D19 k3 尺寸与 derand 前缀对照；本目录默认自生成 ek |

**自包含约束**：[`SELF_CONTAINED.md`](SELF_CONTAINED.md)。

---

## 1. 锁定参数

| 项 | 约定 |
|----|------|
| 参数集 | ML-KEM-768，`k=3` |
| I/O | `ek_kem(1184)` + `m(32)` + LUT → `c(1088)` + `K(32)` |
| KEM 头 | `H(ek)` + `G(m‖H(ek))` 并入 prep 入口前段，不另开 kernel launch |
| PKE | D14 k3：Â[9]、`re[7]`、INTT batch4、`du=10`、`dv=4` |
| Launch | SIM **2**；CPU **5**（分段调试路径） |
| derand | k3 前缀；不做 pad-to-4/8 |

---

## 2. FIPS 增量

```text
输入：ek ∈ B^{1184}，m ∈ B^{32}
(K, r) ← G(m ‖ H(ek))        // device prep 前段
c ← K-PKE.Encrypt(ek, m, r)   // D14 k3 Encrypt
输出：(c, K)
```

| 量 | 字节 | 位置 |
|----|------|------|
| `ek` | 1184 | `input/ek_kem.bin`（= `ek_PKE`） |
| `m` | 32 | `input/m.bin` |
| `K` | 32 | `output/K.bin` |
| `r`/`coins` | 32 | workspace GM；device 写，不作为生产输出 |
| `c` | 1088 | `output/c.bin` |

---

## 3. Launch 拓扑

```text
Host：读 ek_kem、m、LUT；分配 coins/a_hat/re/K workspace
  │
  ├─ Launch-1: f203_kem_enc_prep
  │     block0: KemEncInitHead(ek,m → K,coins)
  │     AIV0/1: BuildEncryptPrepSinglePipe（Â[9] + re[7]）
  │
  └─ Launch-2: f203_encrypt_l18_l19
        D14 k3 compute + e₂+=μ + inline tail pack → c
Host：D2H c、K
```

CPU 仍沿用 D14 分段 compute：`prep_kem → ntt_y → at_jp → intt_e1 → pack`，其中 `golden_v.bin` 仅为 CPU 注入数据。

---

## 4. 接线清单

| 路径 | 内容 |
|------|------|
| `cmake/encaps/CMakeLists.txt` | `ENCRYPT_ROOT` 指向 D14 k3；kernel 文件 = 本目录 prep entry + D14 compute 五段 |
| `kem/f203_kem_enc_init.hpp` | 设备头段：读 `m_gm`、算 `H/G`、写 `K`/`coins` |
| `main_kem_encaps.cpp` | host 编排：SIM 2 / CPU 5，输出 `c` 与 `K` |
| `scripts/gen_data.py` | 本地生成 k3 `ek/m/LUT/golden_v/golden c/K` |
| `scripts/verify_kem_encaps.py` | `c(1088)` / `K(32)` 与 golden 对拍 |

---

## 5. 验收命令

```bash
cd ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg20-kem-encaps-device-k3
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

防挂死预算：`KEM_ENCAPS_KERNEL_BUDGET_SEC` 默认 900 秒；成功后须确认用例根无 stray dump。

---

## 6. 非目标

- 写 `examples/incubating` / `examples/stable`。
- 改 D14 k3 已锁几何。
- 为 k3 补 0 到 k4 布局。
