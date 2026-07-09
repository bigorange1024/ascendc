# INTEGRATION_PLAN — pass-fix-f203-alg14-lines3-15-encrypt-prep-k4

**定位**：`ascendc-tests/` **Alg.14 Encrypt 设备采样段探针**（行 3–15 设备侧：Â + r/e₁/e₂）；**非** `examples/` 交付、**非** 全链 Encrypt。

**代码来源约束（强制）**：

| 允许 | 禁止 |
|------|------|
| [`stable-fips203-mlkem-pke-keygen-k4`](../../examples/stable/stable-fips203-mlkem-pke-keygen-k4/)（**优先**） | [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) **任何源码** |
| `examples/incubating/exp-*`、`ascendc-tests/pass-*` 活跃探针 | `ascendc-tests/frozen/`、`fix-f203-alg*`（除本目录）照抄 |
| `library/shared/` | |

**实现基线**：stable KeyGen prep 单 TPipe（`BuildKeygenPrepSinglePipe`）参数化为 Encrypt：`ρ` 自 `ek_pke` 尾 32B；PRF+CBD batch **9**、`coins` 作密钥。

---

## 1. 目标与边界

FIPS 203 **Algorithm 14**（ml_kem_1024 / k=4）本探针覆盖段：

| 算 | 不算 |
|----|------|
| 行 3–7：`ρ`（自 `ek`）→ 16× SampleNTT → `a_hat[16,256]`（GM 布局同 KeyGen **A**，Âᵀ 在后续 compute 读索引转置） | 行 2：`t̂ ← ByteDecode₁₂(ek)` |
| 行 8–15：`coins` → PRF+CBD → `r[4,256]`、`e₁[4,256]`、`e₂[256]` | 行 16–17：NTT(r) |
| **单 launch** `f203_encrypt_prep`（对齐 stable KeyGen prep） | 行 18+ 线性层、Compress、落盘 `c` |

---

## 2. I/O 契约

| 路径 | 尺寸 | 说明 |
|------|------|------|
| `fixtures/ek_pke.bin` | 1568B | **权威副本**（stable `SEED_D=20260619` 一次拷贝）；`gen_data.py` → `input/` |
| `input/ek_pke.bin` | 1568B | 运行态（由 fixtures 复制） |
| `input/coins.bin` | 32B | `gen_data.py` 由 `COINS_SEED` 派生 |
| `output/a_hat.bin` | 16384B | 设备 `int32[16,256]` |
| `output/re.bin` | 9216B | 设备 `r‖e₁‖e₂` 扁平 `int32[9,256]` |
| `output/golden_a_hat.bin` | 同上 | host oracle |
| `output/golden_re.bin` | 同上 | host oracle |

**ρ 提取**：`ρ = ek_pke[1536:1568]`（设备 GM 读，不经 Host 注入 ρ）。

---

## 3. 数学（k=4 锁定）

### 3.1 Â（同 KeyGen GM）

```text
对每个 (p,j) ∈ {0..3}²:
  a_hat[p,j] ← SampleNTT( ρ || byte(j) || byte(p) )
flat(p,j,c) = (p*4 + j)*256 + c
```

### 3.2 r / e₁ / e₂（ml_kem_1024：η₁=η₂=2）

```text
对 nonce n = 0..8:
  buf ← PRF(coins, n) = SHAKE256(coins || byte(n))，squeeze 128B
  poly ← SamplePolyCBD_η=2(buf)
nonce 0..3 → r；4..7 → e₁；8 → e₂
```

---

## 4. 设备编排

```text
Launch 1  f203_encrypt_prep  blockDim=2  AIV_ONLY
  Phase A（双 AIV）：BuildAHat16ShardWithUb(ρ from ek, …)
  Phase B（block0）：PRF batch=9(coins) → CBD batch=9 → re GM
  block1：PipeBarrier 等待 block0
```

vendoring：`scripts/vendor_sync_from_stable_keygen.sh` → `prep/{ahat,alg7,alg8,presample}/`。

探针自有：`f203_encrypt_prep_{layout,ub,entry}.cpp`、`f203_encrypt_re_{prf,cbd}.hpp`。

**PRF host tiling**：`FillShakeTiling(batch=8, maxMsgLen=64, outLen=128, rate=SHAKE256)`；maxMsgLen 须等于 `PRF_MSG_STRIDE`（64），不可用 33。nonce 8（e₂）单独一条 SHAKE。

---

## 5. Golden

- **入口**：`scripts/gen_data.py` → `scripts/golden_encrypt_prep.py`（**自包含**，不 import 其它用例 / `library/shared`）
- **几何**：`scripts/prep/alg7_geom.py`（vendored 随 `prep/`）
- **ek**：`fixtures/ek_pke.bin` 固定向量 → 复制到 `input/ek_pke.bin`

---

## 6. 验收

```bash
cd ascendc-tests/pass-fix-f203-alg14-lines3-15-encrypt-prep-k4
bash scripts/vendor_sync_from_stable_keygen.sh
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

- `a_hat`、`re` 均 `max_abs_diff=0`
- **勿**与全链 Encrypt G5 tick 混比

---

## 7. 后续扩展（本阶段不写码）

1. 加 `decode_t_hat(ek)` launch  
2. NTT(r)  
3. 并入 stable `exp-fips203-mlkem-pke-encrypt-k4` 全链
