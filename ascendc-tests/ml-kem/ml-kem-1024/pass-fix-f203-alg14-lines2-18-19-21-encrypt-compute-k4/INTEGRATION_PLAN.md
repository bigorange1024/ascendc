# INTEGRATION_PLAN — pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4

**定位**：`ascendc-tests/` **Alg.14 Encrypt 线性 compute 段**研究探针——设备实现 FIPS 203 **行 2、18、19、21**（**不含 μ**；时域只加 **e₁、e₂**）。

**符号（与 FIPS 203 Alg.14 一致）**：

| 符号 | 含义 | 本探针 GM / 文件 |
|------|------|------------------|
| **r** | randomness（32B） | 非本探针范围（由 [`pass-fix-f203-alg14-lines3-15-encrypt-prep-k4`](../pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/) 的 `coins.bin` 生成 **y,e₁,e₂**） |
| **y** | polyvec（k=4） | prep 输出 `re` 行 0–3；或 `input/y.bin` |
| **ŷ** | `NTT(y)` | `input/y_hat.bin`（G0+ 默认 oracle 注入；后续可接 NTT launch） |
| **t̂** | 自 ek 解码 | 行 2；`output/t_hat.bin` 或内嵌 `matM` |
| **e₁, e₂** | 时域噪声 | prep `re` 行 4–7 / 8；或 `input/e1.bin`、`input/e2.bin` |

**前置探针**：[`pass-fix-f203-alg14-lines3-15-encrypt-prep-k4`](../pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/)（行 3–15：Â、y、e₁、e₂）。

**代码来源约束**：

| 允许 vendoring / 参照 | 禁止 |
|----------------------|------|
| [`stable-fips203-mlkem-pke-keygen-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-keygen-k4/) `compute/`（MIX S1–S3、CrossCore） | [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/) **源码照抄** |
| [`pass-fix-f203-alg11-12-innerproduct-k4`](../pass-fix-f203-alg11-12-innerproduct-k4/)、[`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../pass-fix-f203-stage123-ntt-intt-polyvec8-vec/) | `ascendc-tests/frozen/` 带出 |
| [`pass-f203-alg6-bytedecode-d-vec-k4`](../pass-f203-alg6-bytedecode-d-vec-k4/)（**d=12** 路径） | 运行时 `sys.path` 到其它用例 |

---

## 1. 目标与边界

### 1.1 覆盖（FIPS 203 K-PKE.Encrypt，k=4）

| 行 | 数学（标准用词） | 本探针 |
|----|------------------|--------|
| **2** | **t̂ ← ByteDecode₁₂(ek_PKE)** | 设备 `f203_decode_t_hat`（AIV） |
| **18** | **û ← Âᵀ ∘ ŷ**；**tr̂ ← t̂ᵀ ∘ ŷ**（NTT 域，同 r̂=ŷ） | 单核 **at_y5**（kP=5 内积，见 §3） |
| **19** | **u ← INTT(û) + e₁** | MIX **INTT** + 时域加噪（§4） |
| **21** | **v ← INTT(tr̂) + e₂**（**不加 μ**） | 与行 19 **同一次 INTT batch**；第 5 行 + e₂ |

### 1.2 不覆盖

| 项 | 说明 |
|----|------|
| 行 3–7、8–15 | prep 探针 |
| 行 16–17 `ŷ ← NTT(y)` | G0 用 `y_hat.bin` 注入；G_ntt 子 gate 可后接 |
| μ / `EmbedMessage(m)` | 探针期 **e₂' = e₂**；μ 由 host golden 或后续 tail launch |
| Compress / ByteEncode **c** | 后续 stable encrypt |

### 1.3 验收口径

- Golden **仅验 I/O**（`u.bin`、`v.bin` 或合并 `uv.bin`）；不要求与 correctness 全链 tick 对比。
- CPU + `SIM_DIRECT=1` 双模式；kernel 防挂死预算见 `run.sh`（默认 600s，非 15s 全局门禁）。

---

## 2. 数学与数据契约

### 2.1 行 18 — 融合矩阵 matM（一次内积，两次语义）

不物理转置 **Â**；**Âᵀ[p,j] = A[j,p]**（与 KeyGen 同行主序 GM）：

```text
flat_A(p,j,c) = (p*K + j)*N + c

matM[j, p, ·]  行主序 (j*kP + p)*N + c ,  kK=4, kP=5, N=256
  p ∈ [0..3] : matM[j,p] = A[j,p]     （即 a_hat_offset(j,p)）
  p = 4      : matM[j,4] = t̂[j]

对 p_out ∈ [0..4]:
  acc[p_out] = Σ_{j=0}^{3} MultiplyNTTs( matM[j,p_out], ŷ[j] )
  uTr[p_out] = mod_q(acc[p_out])

uTr[0..3] = û     （Alg.14 行 18）
uTr[4]    = tr̂    （行 19/21 的 INTT 输入，非最终 v）
```

**工程命名**：内积核建议 **`f203_encrypt_at_y5`**（避免与 randomness **r** 混淆）；数学等价于 correctness 探针文档中的 `at_r5`（**5 列输出**，非 32B 种子）。

### 2.2 行 19 / 21 — 统一 INTT + 时域加噪

```text
time[5,N] = INTT_batch( uTr[5,N] )     # 同一 Stage1–3 流，INTT LUT

u[p] = ( time[p] + e₁[p] ) mod q       p = 0..3
v    = ( time[4] + e₂ ) mod q          # 探针期无 μ
```

**噪声布局**（与 prep `re[9,256]` 对齐，便于日后拼全链）：

```text
e_noise[5,N]:
  row 0..3 ← e₁
  row 4    ← e₂ 扩成 1 poly（仅 [0:N) 有效）
```

### 2.3 行 2 — t̂ 生成

```text
ek_PKE[0:1536] = ByteEncode₁₂(t̂)   # 4 × 384B
t̂[j] ← ByteDecode₁₂( ek[ j*384 : (j+1)*384 ] )   # → int32[256]
```

- **合法来源**：[`pass-f203-alg6-bytedecode-d-vec-k4`](../pass-f203-alg6-bytedecode-d-vec-k4/) **d=12** 向量路径（与 [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/) round-trip）。
- **Golden**：host `ByteDecode₁₂`（与 KeyGen `ek` 前半互逆）；输入 `fixtures/ek_pke.bin` 与 prep 探针同源（`SEED_D=20260619`）。

---

## 3. 重点一：t̂ 生成策略

| 阶段 | 做法 | Launch |
|------|------|--------|
| **G0** | golden 直接写 `t_hat.bin`；设备不跑 | 0 |
| **G4** | `f203_decode_t_hat(ek)` → `t_hat[4,256]` | **+1** AIV |
| **G5+** | 设备读 `ek`；decode 可与主 MIX **同进程顺序**（先 decode 再拼 matM） | 并入 §5 |

**matM 拼装**：

| 方式 | 适用 | 说明 |
|------|------|------|
| **Host**（G1 早期） | 调试 | D2H `a_hat`+`t_hat` 拼 `matM.bin`；仅验 at_y5 |
| **Device AIV**（G5+） | 少 launch | block0：`decode` + 写 `t_hat` GM + 填 `matM` 列 4；列 0–3 从 `a_hat` GM 按 `(j,p)` 索引拷贝 |

**禁止**：运行时从 `examples/stable/.../output` fallback 读 `ek`（自包含 `fixtures/ek_pke.bin`）。

---

## 4. 重点二：AIC/AIV 分工与段间衔接

### 4.1 两段本质（与 KeyGen mmad 对比）

| 段 | 算子 | 占核 | KeyGen 对照 |
|----|------|------|-------------|
| **行 18** | NTT 域 basemul 累加（at_y5） | **纯 AIV**（Alg.11 UB） | mmad 内 **Hat** 段；**不经过 AIC MMAD** |
| **行 19/21** | INTT(uTr) | **MIX 1×AIC + 2×AIV**（Tag5T S1–S3） | 同 [`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../pass-fix-f203-stage123-ntt-intt-polyvec8-vec/) |
| **+e₁/e₂** | 时域 mod-q 加 | **AIV**（可在 MIX 核末尾） | KeyGen 在 **NTT 域**加 ê；Encrypt 在 **INTT 之后** |

**结论**：行 18 **不能**塞进 Stage2 MMAD（那是 INTT 对 **NTT 域系数** 的矩阵乘，输入布局不同）。正确衔接是 **顺序流水线**，不是 KeyGen 式「NTT(y) 与 MMAD(ŝ) 绑死」。

### 4.2 INTT（k=5）在 MIX 内的数据流

参照 stage123 **8-poly** 探针（`1×AIC + 2×AIV`，`blockDim=1`），本探针 **k_batch=5**：

```text
uTr[5,256] int32  NTT 域 canonical
    │
    ▼  AIV Stage1（双 AIV 分片，poly-batch）
S0 [2*k, 256] int8  紧凑 [HI_k, LO_k]，k=5 → rowsL=10（无插零）
    │  CrossCoreSetFlag / WaitFlag
    ▼  AIC Stage2
MMAD( S0, LUT_intt ) → mat_c 平面
    │  CrossCore
    ▼  双 AIV Pack + Stage3 merge/mod（INTT 语义）
time[5,256] int32  时域
```

**双 AIV 分片（k=5）**：与 ML-KEM poly-batch 一致，每 AIV 握完整 poly 系数（**禁止** hi/lo 拆到不同 AIV）。

| AIV | poly 索引（建议） |
|-----|-------------------|
| AIV0 | 0, 1, 2 |
| AIV1 | 3, 4 |

（具体以 `AivSplitPolyBatch` 与 `usedCoreNum=2` 闭包为准；**k=5 非 8** 须在 vendored `tiling.h` 增 **`kBatch=5`** 或 **pad 到 k=8**（后 3 行零填充）——实现前二选一锁定，默认 **pad→8** 复用 stage123 二进制最少改动。）

### 4.3 行 18 → 行 19/21 衔接（驻留 vs GM）

| 策略 | 做法 | Launch 数 | 风险 |
|------|------|-----------|------|
| **P-分离（G1–G3）** | at_y5 写 `uTr` GM → INTT 读 GM | 2 | 易调试；多一次全量 GM 往返 |
| **P-融合（目标）** | 单 MIX 核：AIV at_y5 写 **scratch/GM** → 同核 `PipeBarrier` → AIV Stage1 读 **同一缓冲** | **1** | UB 预算须一次算清；见 §5 |

**红线**（[`F203-2s1e-NTT内积UB融合技术总结.md`](../../docs/notes/F203-2s1e-NTT内积UB融合技术总结.md)）：INTT 的权威输入必须是 at_y5 **当次** 写出的 `uTr`；禁止「debug dump 到另一 GM 再读」冒充融合。

### 4.4 INTT 完 → +e₁/e₂ 衔接

```text
AIV Stage3 输出 time[5,N] 在 UB 或 GM
    → 逐行 Load e_noise[row]（或 e₁/e₂ 分 GM）
    → vector Add + mod_q
    → CopyOut u[4,N], v[N]
```

- **与行 19/21 同 launch**：在 MIX 核 **AIV 分支** Stage3 之后追加 **NoiseTail**（无 μ 分支）。
- **u 与 v**：同一 `time[4]` 即 v 的时域 preimage；**一次 INTT batch** 覆盖 5 行，避免「û 四次 INTT + tr̂ 一次 INTT」两次 MIX。

---

## 5. 重点三：Launch 方案（少 launch 优先）

### 5.1 目标拓扑（生产向）

```text
┌─────────────────────────────────────────────────────────────┐
│ Launch 1  f203_encrypt_linear  MIX_AIC_1_2  blockDim=1      │
│   [可选 AIV 前缀] decode_t_hat(ek)→t_hat；拼 matM 列 4      │
│   AIV: at_y5(matM, y_hat) → uTr[5,256]                      │
│   sync                                                        │
│   AIV: INTT Stage1 (k=5 或 pad 8)                             │
│   AIC: INTT Stage2 MMAD                                       │
│   AIV: INTT Stage3 + NoiseTail(+e₁,+e₂) → u, v                │
└─────────────────────────────────────────────────────────────┘
```

**Launch 数 = 1**（`ek`、`a_hat`、`y_hat`、`e_noise`、LUT 已在 GM；prep 另计）。

### 5.1.1 关键可行性：行 18（Âᵀ|t̂ᵀ）× ŷ 的 5 行内积如何融入现有 halfrows

本仓现有行 19 内积实现（当前探针 `encrypt_at_jp`）是 **halfrows**：双 AIV 各自计算 2 行输出 `p∈{0,1}` / `{2,3}`，每行长度 `N=256 int32`。
扩展到 `kP=5` 时，如果强行把 `kPPerAiv=kP/2` 改成 2 或 3，会牵动 scratch 预算、布局和 AIV 负载划分。
为避免改动面过大，本探针 **推荐**保持 halfrows 不变，并在单 launch fused 内核里采用“追加一行”的最小改动方案：

| AIV | 计算行（p_out） | 输入 fPoly 来源 |
|-----|------------------|----------------|
| AIV0 | 0,1（halfrows 原样） | `A[j,0]`、`A[j,1]` |
| AIV1 | 2,3（halfrows 原样） | `A[j,2]`、`A[j,3]` |
| AIV0（追加） | 4（仅 1 行） | `t_hat[j]` |

数学一致性：

```text
对 p_out ∈ [0..3]:
  fPoly = A[j,p_out]   （GM 上 A 仍是 [j,p] 行主序）
对 p_out = 4:
  fPoly = t_hat[j]     （行 2 decode 产物，等价 matM[j,4]）
gPoly = y_hat[j]
row = MultiplyNTTs(fPoly,gPoly)  # 复用 alg11_ub::compute_on_ub
acc[p_out] += row
最后一次 mod_q(acc) 得到 uTr[p_out]
```

工程注意：

- **32B 对齐**：每行 `N=256 int32 = 1024B`，GM 行起始地址与 DataCopy 长度天然满足 32B 对齐；`kP=5` 不需要 pad 到 8 才能对齐。
- **同步**：追加行 `p=4` 必须写入与 INTT Stage1 相同的权威输入缓冲（UB 驻留或对应 GM scratch）；禁止写到仅用于 dump 的 GM 再由同核 MTE 读回。
- **负载不均**：AIV0 多算一行，但这是可行性阶段最小改动；后续若追求性能，再考虑 `kP=5` 原生 split 或 pad→8。

若 decode 与 MIX **UB 预算冲突**，退化为 **2 launch**：

```text
Launch A  f203_decode_t_hat     AIV_ONLY  blockDim=1
Launch B  f203_encrypt_linear    MIX       （无 decode 前缀）
```

仍优于 correctness 全链 **6+ launch**。

### 5.2 与 prep 全链拼接

```text
Launch P   f203_encrypt_prep          AIV×2   （已有）
Launch C   f203_encrypt_linear        MIX×1   （本探针）
─────────────────────────────────────────────
合计 2 launch（Encrypt 核心，不含 μ/compress）
```

`ŷ` 来源二选一（须锁定）：

| 选项 | 说明 |
|------|------|
| **A（推荐 G0）** | prep 产出 `y` → 本探针入口前 **host/golden NTT** → `y_hat.bin` |
| **B（G_ntt）** | 增加 `f203_encrypt_ntt_y` MIX k=4（vendored stage123），**+1 launch** → 共 3 launch |

### 5.3 分阶段 Gate（实现顺序）

| Gate | 设备内容 | 输入 | 验证 |
|------|----------|------|------|
| **G0** | 无（golden only） | `matM,y_hat,e_noise` bin | `u,v` oracle |
| **G1** | `at_y5` | 上 + `output/u_tr.bin` | `u_tr` max=0 |
| **G2** | INTT batch | `u_tr` + LUT | `time` max=0 |
| **G3** | NoiseTail | `time` + `e_noise` | `u,v` max=0 |
| **G4** | `decode_t_hat` | `ek_pke` | `t_hat` max=0 |
| **G5** | Launch B（线性 MIX） | 全 bin / prep 衔接 | `u,v` max=0 |
| **G6** | Launch A+B 或 **单 Launch 1** | `ek` + prep 输出 | 同上 |

---

## 6. I/O 与目录（草案）

```text
pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/
  INTEGRATION_PLAN.md
  STATUS.md
  fixtures/ek_pke.bin          # 同 prep（1568B）
  input/
    ek_pke.bin                 # 可选（G4+）
    a_hat.bin                  # 16384B 或改用 matM.bin
    matM.bin                   # 4*5*256*4 B（G1+ 推荐）
    y_hat.bin                  # 4096B
    e_noise.bin                # 5*256*4 B（或 e1.bin + e2.bin）
    lut_intt_even/odd_stacked.bin
  output/
    t_hat.bin                  # G4
    u_tr.bin                   # G1
    u.bin                      # 4*256*4
    v.bin                      # 256*4
  scripts/
    gen_data.py                # 自包含 golden
    golden_linear.py           # 行 2/18/19/21 host oracle
  compute/                     # 实现阶段再 vendoring
```

**Golden 登记**：`ByteDecode₁₂`、`NTT/INTT`（stage123）、`MultiplyNTTs` 累加须落在 [`docs/specs/fips203-baseline-registry.md`](../../docs/specs/fips203-baseline-registry.md) 已验证 API；缺项先停 G5。

---

## 7. 风险与决策点

| 风险 | 缓解 |
|------|------|
| k=5 INTT 与 stage123 k=8 几何不一致 | 默认 **pad 到 8**；golden 对 pad 行写 0 |
| 单 MIX 核 UB 超限 | 先 G1∥G2 拆 launch，再合并 |
| SIM `func_key` 上限 | 单核 `f203_encrypt_linear`；**禁止**再拆 `at_y`+`intt`+`noise` 三个 AIV 注册 |
| 符号混用 `r`/`y` | 文档与新增代码一律 **y/ŷ**；legacy `re.bin` 仅作文件别名 |
| correctness 探针 507000 / 多 session | 本探针 **不继承**其 launch 编排；仅借鉴 **at_y5 数学** |

**待用户锁定**：

1. INTT batch：**k=5 原生** vs **pad k=8**（默认 pad k=8）。
2. G0 是否强制 **预生成 `matM.bin`**（跳过设备拼 matM）。
3. 是否与 prep **同二进制**（`ENCRYPT_BUILD_PROFILE`）还是独立 CMake target。

---

## 8. 实现进展

**权威状态**：见 [`STATUS.md`](STATUS.md)（2026-07-07 起分平台表述）。摘要：

| 平台 | 判定 | 行 2/18/19/21 |
|------|------|---------------|
| SIM 默认单 launch | **完成** | 全在 `f203_encrypt_l18_l19` 验收（kP=5、INTT k=8、v） |
| CPU 三 launch | **部分对照** | û/u 子集；tikicpu 不得调融合核（MIX 死锁） |

### 8.1 Launch 拓扑（定案）

| 模式 | Kernel 序列 | CPU | SIM |
|------|-------------|-----|-----|
| **CPU（固定）** | `ntt_y` → `at_jp` → `intt_e1` | ✓ `RunCpuThreeLaunch` | — |
| **SIM 默认** | `f203_encrypt_l18_l19` ×1 | —（tikicpu 死锁） | ✓ `RunSimFusedSingleLaunch` |
| **SIM 调试** | 同 CPU 三序列 | — | ✓ `ASCENDC_SIM_HOST_MODE=phased_launch` |

### 8.2 单 launch 数据流（定案）

对齐 §4.3 **P-融合** 与 [`F203-Encrypt-compute-行18-19-UB驻留技术总结.md`](../../docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md)：

```text
NTT(y) MIX S1–S3 → y_hat GM
双 AIV: innerproduct_halfrows_to_ub(Â, ŷ) → ubUNtt (UB)
双 AIV: AivK8Split::ProcessFromLocal(ubUNtt)     # INTT S1，禁止 GM 读 û
[对拍] dump_u_ntt_halfrows_ub → u_ntt GM         # DataCopy UB→GM only
CrossCore: ST_IP_AIV_DONE=4 → ST_AT_JP_GATE=8
AIC: INTT S2 MMAD (flag 1/3)
双 AIV: INTT S3 + 时域 +e₁ → u GM
```

**红线**：禁止标量写 `uNtt` GM 后同 kernel MTE `DataCopy(GM→UB)` 作 Stage1 输入（SIM 不可见 → `u≈e₁`）。

### 8.3 关键源码

| 路径 | 作用 |
|------|------|
| `compute/f203_encrypt_ntt_y_kernel.cpp` | 行 18 NTT(y) |
| `compute/f203_encrypt_at_jp_kernel.cpp` | 行 19 内积（3 launch） |
| `compute/f203_encrypt_intt_e1_kernel.cpp` | INTT + e₁（3 launch） |
| `compute/f203_encrypt_l18_l19_kernel.cpp` | 单 launch 融合 FSM |
| `compute/f203_encrypt_at_jp_scalar.hpp` | `innerproduct_halfrows_to_ub` / `dump_u_ntt_halfrows_ub` |
| `compute/aiv_func.hpp` | `AivK8Split::ProcessFromLocal` |
| `main.cpp` | `RunCpuThreeLaunch` / `RunSimFusedSingleLaunch` / `RunSimThreeLaunch`（调试） |
| `scripts/gen_data.py` | golden |

### 8.4 待办（相对 §5 目标拓扑）

1. **at_y5 kP=5**：`matM` 第 5 列 `t̂`；输出 `uTr[5]`。
2. **INTT batch k=5**（或 pad→8）+ **NoiseTail e₂** → `v`。
3. **行 2** `decode_t_hat` + 设备拼 `matM`。
4. **与 prep 拼接**：`pass-fix-f203-alg14-lines3-15-encrypt-prep-k4` + 本探针 → 2 launch Encrypt 核心。
5. **单 launch 含 v**：UB 预算评估；必要时维持 2 launch。

### 8.5 验收（当前）

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4   # SIM 默认单 launch
# ASCENDC_SIM_HOST_MODE=phased_launch SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

预期：`y_hat`、`u_ntt`、`u` 与 golden `max_abs_diff=0`。

---

## 9. 参考文档

- [F203-innerproduct-k4-技术总结.md](../../docs/notes/F203-innerproduct-k4-技术总结.md)
- [F203-2s1e-NTT内积UB融合技术总结.md](../../docs/notes/F203-2s1e-NTT内积UB融合技术总结.md)
- [F203-Encrypt-compute-行18-19-UB驻留技术总结.md](../../docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md)
- [F203-polyvec8-stage123-NTT-INTT技术总结.md](../../docs/notes/F203-polyvec8-stage123-NTT-INTT技术总结.md)（若存在；否则 stage123 `STATUS.md`）
- prep：[INTEGRATION_PLAN.md](../pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/INTEGRATION_PLAN.md)

---

## 10. 验收命令（实现后）

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

预期：`u`、`v` 与 golden `max_abs_diff=0`（无 μ）。
