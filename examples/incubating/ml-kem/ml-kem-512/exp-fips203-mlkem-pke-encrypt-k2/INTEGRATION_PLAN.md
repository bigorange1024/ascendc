# INTEGRATION_PLAN — exp-fips203-mlkem-pke-encrypt-k2

**定位**：ML-KEM-512 W2/D14，FIPS 203 Alg.14 完整 K-PKE.Encrypt device 探针。输入 `ek_pke[800] + m[32] + coins[32]`，输出仅 `c[768]`。

**状态**：2026-07-27 **CPU + `SIM_DIRECT=1` sim PASS**；SIM tick **338153**；根目录 0 stray dump。详见 [`STATUS.md`](STATUS.md)。

**参数来源**：[`docs/specs/fips203-mlkem512-parameter-card.md`](../../../docs/specs/fips203-mlkem512-parameter-card.md) §3.2。以下数字视为锁定值，遇阻不得改参硬闯。

---

## 1. 数学全链

```text
# prep（launch 1，AIV_ONLY blockDim=2）
rho <- ek_pke[768:800]
forall (j,p) in {0,1}^2:
  a_hat[j,p] <- SampleNTT(rho || byte(j) || byte(p))      # 4 poly
forall n in [0,4]:
  poly_n <- SamplePolyCBD_eta2(PRF(coins, n))
r[0..1] <- poly_0..1
e1[0..1] <- poly_2..3
e2 <- poly_4

# compute+tail（launch 2，MIX blockDim=1）
t_hat <- ByteDecode_12(ek_pke[0:768])
r_hat <- NTT(r)                                           # k=2
u_hat[j] <- InnerProduct(a_hat[j,*], r_hat[*])             # j=0..1
tr_hat <- InnerProduct(t_hat[*], r_hat[*])
(u0,u1,v,空槽) <- INTT batch=4(u_hat0,u_hat1,tr_hat,0)     # 物理 polyvec4；语义不零垫噪声
u <- u + e1
v <- v + e2 + Decompress_1(ByteDecode_1(m))
c1 <- ByteEncode_10(Compress_10(u))                        # 640B
c2 <- ByteEncode_4(Compress_4(v))                          # 128B
c <- c1 || c2                                              # 768B
```

---

## 2. GM 与 launch 契约

| 区域 | 尺寸 | 说明 |
|------|------|------|
| `ek_pke` | 800B | 前 768B 为 `t_hat` 编码，末 32B 为 `rho` |
| `a_hat` | `4*256*sizeof(int32)` | prep 写，compute 读；布局 `(j*K+p)*N` |
| `re` | `5*256*sizeof(int32)` | `r[2] || e1[2] || e2[1]` |
| `u_tr` | `4*256*sizeof(int32)` | compute 内部：`u0,u1,v,空槽` |
| `c` | 768B | 唯一 D2H 产物 |
| `ws_compute` | `296960`B | compute+tail scratch，含 LUT/S0/mat_c 等 |

SIM/生产只跑两次 launch：

1. `f203_encrypt_prep`：AIV_ONLY `blockDim=2`，AIV 分片 `a_hat` 为 **2+2**；PRF/CBD **5** poly 仅 block0 写。
2. `f203_encrypt_l18_l19`：MIX `blockDim=1`，NTT(r) k=2、inner、INTT batch=4、tail pack 内联完成。

CPU 路径保留 5 段调试：prep -> `ntt_y` -> `at_jp` -> `intt_e1` -> `alg14_pack`；CPU pack 注入 `input/golden_v.bin`，不代表 SIM/生产路径。

---

## 3. 文件结构

```text
exp-fips203-mlkem-pke-encrypt-k2/
├── f203_encrypt_prep_*        # launch 1：rho->a_hat，coins->re
├── f203_encrypt_full_layout.h # 全链 GM arena
├── f203_encrypt_*_layout.h    # prep / tail / compute-tail 常量
├── compute/                   # launch 2 与 CPU 分段 helper
├── prep/                      # k2 prep 组件（alg7/alg8/ahat/presample）
├── scripts/gen_data.py        # 自包含 k2 input/golden 生成
└── scripts/host_golden/       # Python oracle（I/O golden，不是 AscendC 规格）
```

---

## 4. Golden

`scripts/gen_data.py` 本地生成：

| 文件 | 尺寸 | 说明 |
|------|------|------|
| `input/ek_pke.bin` | 800B | `gen_ek_pke.py` 生成 ML-KEM-512 public key |
| `input/m.bin` | 32B | 固定 `SEED_D=20260619` 派生 |
| `input/coins.bin` | 32B | 同上 |
| `golden/c.bin` | 768B | Python Alg.14 oracle |
| `input/golden_v.bin` | 1024B | CPU 分段 pack 注入；SIM 不依赖 |

derand 前缀使用参数卡锁定的 `exp-mlkem-f203-2s1e-k2:SEED_D=`；golden 仅作为黑盒 I/O oracle。

---

## 5. Gate

| Gate | 验收 | 状态 |
|------|------|------|
| S0 | k3 活跃 device tree 复制到 k2；删除 build/out/input/output/OPPROF 等产物 | ✓ |
| S1 | retarget k=2：`ek=800`、`c=768`、`a_hat=4`、`re=5`、d=10/4、INTT batch=4 | ✓ |
| PASS | `bash run.sh -r cpu -v Ascend910B4` 与 `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` 均 `c max=0` | ✓ tick **338153** |

---

## 6. 禁止项

- 不从 `frozen/` 复制源码、customspec 或路线。
- 不把 INTT batch 改成 6/8，也不写零 poly 凑 k4 几何。
- 不把 `u/v/a_hat/re` 作为默认输出落盘。
- 不晋级 `examples/stable`；PKE exp 需另走 customspec。
