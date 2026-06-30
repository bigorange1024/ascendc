# INTEGRATION_PLAN — fix-f203-alg15-pke-decrypt-correctness-k4

**定位**：`ascendc-tests/` **Alg.15 PKE Decrypt 设备全链拼装探针**（ml_kem_1024 / k=4）；对齐 [`fix-f203-alg14-pke-encrypt-correctness-k4`](../fix-f203-alg14-pke-encrypt-correctness-k4/) 治理模式。

**参照 Encrypt 终态**：单 ACL session、Host 仅 I/O + LUT 搬运、密码学全在 AI Core。

---

## 1. 目标与不变量

FIPS 203 **Algorithm 15**（ml_kem_1024 / k=4）：

```text
input:  dk_PKE (1536B = ByteEncode₁₂(ŝ) polyvec)
        c (1568B = c₁ ‖ c₂)
output: m (32B)
```

| 不变量 | 说明 |
|--------|------|
| **设备全链** | 默认 `run.sh` 密码学在 **AI Core**；Host 只写 input、读 output、搬 LUT |
| **拼装来源** | 活跃探针 **vendor_sync 复制**到本目录 + `library/shared` |
| **Golden** | `scripts/host_golden/` **仅** `DECRYPT_VERIFY=1` 对拍；禁止 liboqs |
| **I/O 锁定** | dk=1536B、c=1568B（c₁=1408B d_u=11、c₂=160B d_v=5）、m=32B |

**数学链**（canonical mod q=3329）：

```text
u  ← Decompress₁₁(ByteDecode₁₁(c₁))     // 4 poly
v  ← Decompress₅ (ByteDecode₅ (c₂))     // 1 poly
û ← NTT(u)
ŵ ← Σⱼ MultiplyNTTs(ŝ[j], û[j])
w  ← v − INTT(ŵ)
m  ← ByteDecode₁(Compress₁(w))
```

---

## 2. 已验收积木

| Alg.15 段 | 来源探针 | 本目录 |
|-----------|----------|--------|
| ByteDecode₁₂ → ŝ | Encrypt `decode_t_hat` 同构 | `prep/decode_dk/` |
| ByteDecode_d + Decompress_d (11/5) | Encrypt `pack` 逆运算 | `unpack/` |
| NTT polyvec k=4 | `pass-fix-f203-stage123-ntt-intt-polyvec8-vec` | `compute/ntt_u/` |
| k 元内积 ŝ·û | `pass-fix-f203-alg11-12-innerproduct-k4` + alg11 | `compute/su_dot/` |
| INTT + Compress₁ + 提 m | INTT 探针 + 标量尾 | `compute/final/` |

---

## 3. Gate（G4 生产）

| Gate | 设备路径 | 验收 |
|------|----------|------|
| G0 | marker | launch 成功 |
| G1 | unpack c → u,v | vs golden |
| G2 | decode dk → ŝ；NTT u → û | vs golden |
| G3 | su_dot → ŵ | vs golden |
| G4 | final → m.bin | max=0（32B） |

---

## 4. Launch 编排（G4 定稿：2 次 aclrtLaunchKernel）

**原则**：prep 与 NTT 分 launch；NTT 与 INTT 分 kernel（中间 sync）。详见 [`docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md`](../../docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md)。

```text
aclInit / CreateStream
  │
  ├─ Launch-1: f203_decrypt_g4_prep
  │     unpack(c → u,v) + decode(dk → ŝ)     AIV 标量，MIX 占位
  │     aclrtSynchronizeStream
  │
  └─ Launch-2（同 session，两 kernel）:
        f203_decrypt_g4_chain_ntt
          NTT(u → û) + su_dot + pad(ŵ)       MIX + AIV 标量
        aclrtSynchronizeStream
        f203_decrypt_g4_chain_intt
          INTT(ŵ) + extract(v,w → m)         MIX + AIV 标量
        aclrtSynchronizeStream
  D2H 中间张量 + m
aclFinalize
```

| 核 | KERNEL_TASK_TYPE | 说明 |
|----|------------------|------|
| `f203_decrypt_g4_prep` | MIX 占位（仅 AIV 算） | 释 SIM device 路径；CPU 用 AIV_MODE |
| `f203_decrypt_g4_chain_ntt` | MIX 1×AIC+2×AIV | NTT + Alg.11 su_dot |
| `f203_decrypt_g4_chain_intt` | MIX 1×AIC+2×AIV | INTT + Compress₁ 提 m |

**废弃**：6 核多 launch、单 launch `g4_full`（SIM 上 û 或 m 错 / CrossCore 死锁）。

**验收**（2026-06-30）：CPU+SIM G1–G4 max=0；SIM tick **~427k**。

---

## 5. 测试向量

同一 `SEED_D`（默认 20260619）：

1. `gen_dk_pke.py` → `input/dk_pke.bin`（与 Encrypt `gen_ek_pke` 同源 KeyGen golden 的 ŝ 段）
2. `golden_c.py`（Encrypt host_golden 复制语义）→ `input/c.bin`
3. `golden_m.py` → `output/golden_m.bin`

---

## 6. 目录骨架

```text
fix-f203-alg15-pke-decrypt-correctness-k4/
├── INTEGRATION_PLAN.md
├── SELF_CONTAINED.md
├── STATUS.md
├── unpack/
├── prep/decode_dk/
├── compute/{ntt_u,intt_w,su_dot,final,g4_full}/
├── scripts/{gen_data.py,verify_*.py,host_golden/,vendor_sync.sh}
└── main_decrypt*.cpp
```
