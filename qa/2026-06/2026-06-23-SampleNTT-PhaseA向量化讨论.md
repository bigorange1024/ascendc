# 2026-06-23 — Alg.7 SampleNTT / Phase A 向量化实验与讨论

**探针（已冻结）**：[`frozen-fix-f203-alg13-device-presample-a-hat-k4`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/)  
**母探针**：[`pass-fix-f203-alg13-lines8-15-se-k4`](../../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/)（行 8–15 V3，SIM **133153**）  
**计划**：[`A_VECTOR_PLAN.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/A_VECTOR_PLAN.md) · [`PHASE_A_VEC_REJ_PLAN.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/PHASE_A_VEC_REJ_PLAN.md)  
**SIM 表**：[`SIM_BENCHMARK.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/SIM_BENCHMARK.md)  
**业界参考**：liboqs `sampling.c` · `rej_uniform_avx2.c` · `rej_uniform_asm.S` · pq-crystals `kyber768_avx2/indcpa.c`

---

## 1. 背景与约束（拍板）

| 约束 | 内容 |
|------|------|
| **Launch** | `blockDim=1`，**单 AIV**（与母探针 V3 一致） |
| **并行** | **不用** SIMT / 多 AIV |
| **管线** | `SEED_D` → G（ρ+σ）→ **A**（Alg.7 / 行 3–7）→ P（SHAKE256×8）→ C（CBD P1b） |
| **输出** | `a_hat[16,256]` + `src[8,256]`；`a_hat` 行主序 `(p*K+j)*N+c` |
| **Golden** | `scripts/golden_a_hat_sampling.py`（对齐 liboqs `mlk_poly_rej_uniform`） |

**语义澄清**：SampleNTT **不是**设备上再跑蝶形 NTT；是 SHAKE128 XOF + rej_uniform → 256 个 NTT 域系数。

---

## 2. Phase 1：设备标量 A（✅ PASS）

**2026-06-23** fork 母探针，增加 Phase A：

- G 扩展为 `HashGRhoSigma` → **ρ[32] + σ[32]**
- `f203_a_hat_scalar.hpp`：16× `SampleNTT(ρ‖j‖i)`，PermuteChain 标量 SHAKE128
- CPU/SIM `VERIFY_STAGE=all`：**max_abs_diff=0**

| 路径 | SIM tick | 说明 |
|------|----------|------|
| 全标量 Phase A | **719237** | 16× 串行 Keccak + 标量 rej |
| 母探针仅 8–15 | **133153** | 差分 ≈ **586084** 在 Phase A |

**慢的主因**：每 poly ~4–5 次 Keccak-f[1600]；CAModel 对标量 PermuteChain 极慢。  
**否决**：把 `tiny_sha3` 循环版搬进 AIV — SIM 比 PermuteChain **更慢**（非语义问题，是模拟器访存/间接寻址）。

---

## 3. A-v1：batch SHAKE128（✅ 功能 PASS，SIM 负优化）

**目标**：对齐母探针 Phase P — `shake_xof_kernel batch=16, rate=168`。

| 路径 | SIM tick | vs 标量 A |
|------|----------|-----------|
| `SE_A_HAT_STAGE=scalar` | **719237** | — |
| `SE_A_HAT_STAGE=shake_vec`（A-v1） | **918301** | **+27%** |

**原因**：batch XOF 已对拍，但 **16× 标量 rej** + 504B GM 往返 + tail 重 absorb 仍在；batch 调度有固定开销。

**结论**：向量 Keccak **调度验证通过**；tick 未降。瓶颈转到 **rej 与数据搬运**。

---

## 4. A-v2：UB / GM rej 搬运实验

| 变体 | SIM tick | 结果 |
|------|----------|------|
| A-v2 SetValue 版（8064B bulk + UB GetValue/SetValue） | **978341** | 功能 PASS；**+36%** vs 715k 档 |
| A-v2 无 SetValue（xof bulk DC + `RejUniformGm` 直写） | **715537** | 功能 PASS；**当前最快 shake_vec 全段** |

**反模式（CAModel 上极贵）**：

- UB 上 `GetValue` / `SetValue` 逐元素解包/比较/写回
- 假向量：只有外层 48B 循环，内层仍标量 UB 访存

---

## 5. A-v3：惰性 tail squeeze（✅）

- 首批 504B 来自 batch XOF；不足 256 系数时 **续流 squeeze 168B**，避免 per-poly 重 absorb
- 与 liboqs `poly_rej_uniform_x4` 的 `while (any ctr<256) squeeze(1 block)` 对齐
- 单独 tail 路径 SIM 约 **725713**（相对 A-v2 无 SetValue +~1.4%）

---

## 6. A-v4a / A-v4b：48B 块 rej 实验（✅ CPU/SIM PASS，SIM 负优化）

**开关**：`SE_A_HAT_REJ=scalar|vec_a|vec_b`（`vec` 别名 `vec_a`）

| `SE_A_HAT_REJ` | 实现 | SIM tick（G+A+P+C） | vs scalar |
|----------------|------|---------------------|-----------|
| **scalar** | 标量 `RejUniform` + GM 栈 `rowBuf[504]` | **881627** | 基线 |
| **vec_a** | 48B 栈解包 + 标量 compact（`f203_a_hat_rej_vec_a.hpp`） | **960762** | **+9.0%** |
| **vec_b** | 48B + 语义 mask→下标 LUT（`f203_a_hat_rej_vec_b.hpp`） | **1004273** | **+13.9%** |

**CPU**：`SEED_D=20260619` / `SEED_D=14` 均 PASS。

**关键实现点**：

- `f203_a_hat_rej_common.hpp`：栈上解包，**无 UB GetValue**
- v4b LUT：`scripts/gen_rej_uniform_table.py` 生成 **语义表**（mask bit → cand 下标），**不是** x86 `pshufb` 控制字
- 数据路径：8064B `DataCopy` 拉 xof + 每 poly 从 GM 拷 504B 到栈 → rej → 写 `a_hat` GM

**为何 scalar 881627 ≠ 历史 715537**：管线改为「bulk DC + GM 栈 rej」，比 UB 直扫标量 rej **+23%**；**v4 对比必须用 881627**，不能与 715537 直接比。

**为何 v4a/b 更慢**：

1. 48B 主循环仍是 **标量** 解包/compact（未用宽载 Shift）
2. LUT compact 多一层表查，SIM 上无收益
3. 旧版 UB `GetValue` 假 vec 会 **>600s 无输出**；新版栈版约 **10 min** 可跑完 — 属负优化，不是挂死

**决策**：默认 **`SE_A_HAT_REJ=scalar`**；v4a/b 保留作对照，**不继续投 48B 栈块路线**。

---

## 7. Alg.7 算法分解与可向量化段

### 7.1 单 poly SampleNTT 步骤

```text
seed = ρ[32] || byte(j) || byte(i)          // 34 B
SHAKE128.Absorb(seed)
buf ← Squeeze(3 × 168) = 504 B              // GEN_MATRIX_NBLOCKS=3
ctr ← RejUniform(â, 256, 0, buf)
while ctr < 256:
  block ← Squeeze(168)                      // 续流，不 re-absorb
  ctr ← RejUniform(â, 256, ctr, block)
```

`RejUniform` 主循环：每 **3 字节** → 两个 12-bit 候选 `d1,d2`；`< Q(3329)` 才写入。

### 7.2 业界两层套路（liboqs / pq-crystals）

| 层 | 做法 |
|----|------|
| **矩阵级** | K² poly；×4 batch XOF + 共享 tail `while` |
| **rej 级** | 标量 3B 步进 **或** 48B 向量 + Compare + **LUT shuffle/tbl 压缩** |

AscendC 已验证：**XOF batch**（Phase P）、**CBD 向量**（Phase C）。**rej 真向量链**（Compare + Gather + LUT）**尚未 POC 成功**。

### 7.3 Host  profiling 粗估（Python golden）

单次 SampleNTT 时间：**XOF ~95%**，**rej ~5%**。  
→ 设备上 **tail batch XOF** 优先于 rej 微优化；但 CAModel 上 **rej 访问模式**（GM/UB 往返）可放大到与 Keccak 同量级，故搬运形态仍重要。

---

## 8. 讨论：d1/d2 解包的空间换时间（2026-06-23 末）

用户重读 Alg.7 步骤 6–7，提出 **rej 之前** 的 element-wise 段可向量化：

1. 首批 **504 B** = **168 组 × 3 B**（`C0,C1,C2`）
2. 可视为 `[3, 168]`：三行分别为 `C0[c]`,`C1[c]`,`C2[c]`
3. 对每组列做位运算得 **`d1[168]`、`d2[168]`**（各 168 维，**不是 169**）
4. **拒绝采样**（`< q` + 压缩写 `â[j]`）在 d1/d2 **之后**再做

**共识**：

| 难点 | 工程化解 |
|------|----------|
| 168 非 32B 对齐 | UB **padding**（504→512B）或按 **48B/64B 子块**扫 |
| 504 → `[3,168]` reshape | **de-interleave**：Gather / 固定索引表；或在 48B 块内直接产出 `cand[32]`，不必显式存满 `[3,168]` |
| 无 uint8 向量算术 | **uint16/uint32 宽载 + Shift/Mask**；MTE 仍可用 `uint8` `DataCopy` |

**与项目经验对齐**：同 alg8 / NTT stage3 — **大块 DataCopy → UB 布局变换 → 向量 shift/mask → 再写回**；**unpack 与 rej+compact 应分阶段**，不要像 A-v4a/b 那样混在栈标量循环里。

**下一步（未开工）**：

1. **POC**：仅 UB 上 unpack → `d1/d2`（或 `cand[32]`），对拍 golden 中间量
2. PASS 后再接 `Compares` + compact（LUT/Gather 或半向量）
3. 并行推进：**恢复 UB 直扫 scalar rej（~715k 档）** 或 **batch tail XOF** 作为 tick 拐点

---

## 9. 探针门控与复现

```bash
cd ../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4

# 默认：shake_vec + scalar rej
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4

SE_A_HAT_STAGE=scalar bash run.sh -r sim -v Ascend910B4      # 全标量 A
SE_A_HAT_REJ=vec_a|vec_b bash run.sh -r sim -v Ascend910B4    # A-v4 对照
SE_A_HAT_PROBE=xof_only|rej_only bash run.sh -r sim -v Ascend910B4
SEED_D=14 bash run.sh -r cpu -v Ascend910B4
```

SIM 全段约 **7–10 min**（`KERNEL_COMPUTE_BUDGET_SEC` 默认 600–900）。

---

## 10. 待办（交给下一 agent）

| 优先级 | 项 |
|--------|-----|
| P0 | 确认探针目录源码完整性（`run.sh`、`main.cpp`、`f203_a_hat_*.hpp` 等）；若缺失从 backup/git 恢复 |
| P1 | Alg.7 **d1/d2 UB unpack POC** + golden 中间量对拍 |
| P2 | 恢复 **715537 档** scalar rej（减少 GM 栈往返）作公平基线 |
| P3 | **batch tail XOF**（A-v3 深化，去掉 16× 独立 tail absorb） |
| P4 | 阶段 2：链式 / vec-k4-v3 接入设备 `a_hat`（母计划） |

---

## 11. 文档索引

| 文档 | 用途 |
|------|------|
| [`A_VECTOR_PLAN.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/A_VECTOR_PLAN.md) | Phase A 分阶段实现方案 |
| [`PHASE_A_VEC_REJ_PLAN.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/PHASE_A_VEC_REJ_PLAN.md) | rej 向量化设计稿 |
| [`SIM_BENCHMARK.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/SIM_BENCHMARK.md) | tick 权威表 |
| [`docs/notes/F203-Alg7-PhaseA-向量化技术总结.md`](../../docs/notes/F203-Alg7-PhaseA-向量化技术总结.md) | 经验教训（非讨论过程） |

---

## 12. d1/d2 POC 探针 SIM 阻塞解除（同日追加）

**探针**：`../../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4`（Alg.7 单 poly，rej 前 d1/d2 向量段）

**现象**：CPU 全 PASS；SIM 上 xof 对、d1/d2 全 0（或 max_abs_diff≈max(golden)）。

**根因**：CaModel 上 `DataCopy(GM→UB)` 之后对 **VECIN `LocalTensor::GetValue`** 读恒为 0；`DataCopy(UB→GM)` 仍正确。对 `rawUb` 做 EnQue/DeQue 会加剧该问题。解交织若从 UB 读字节会得到全 0 系数，向量段输出亦全 0。

**修复**：
- 解交织改为 **`xofGm.GetValue(3*c+lane)`**（GM 标量读，对齐 CBD 从 GM 取 prf 字节）
- d1/d2 仍 **向量** `Shift/Muls/Add`；写 GM 保持 VECOUT `EnQue/DeQue` → `DataCopy`
- SIM launch 用 `f203_alg7_sample_ntt_d12_do`（与 presample 同构）

**验收（2026-06-23）**：

```bash
cd ../../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# xof + d1 + d2 golden PASS；SIM ~60002 tick
```

**遗留**：Gather+Cast 全向量解交织路线待单独 POC（CPU 已过，SIM 未验）；集成 a-hat-k4 仍待母探针源码恢复。

---

## 13. Alg.7 line 8–15 rej 向量化：Min+mod 掩码与流式 compact（同日追加）

**背景**：d12 POC（`pass-fix-f203-alg7-sample-ntt-k4`）已覆盖 line 6–7；讨论 rej（line 8–15）能否**高性能**向量化。本仓 A-v4a/b 已证「半向量」负优化（+9% / +14% vs 881627）。

### 13.1 语义共识（强制）

| 点 | 结论 |
|----|------|
| **映射未知** | 不能预先知道 `d1[i]` / `d2[i]` 填 `â[z]` 的哪一个；必须按流顺序 `d1[0]→d2[0]→d1[1]→…` 边扫边填 |
| **串行 `j`** | `if d1<q` 则 `â[j]=d1; j++`；`if d2<q and j<N` 则 `â[j]=d2; j++`；**`j=256` 后停止** |
| **`j<N` 对 d2** | 当 `j=255` 且 `d1` 通过时，**同 pair 的 `d2` 即使 `<q` 也必须丢弃 |
| **多块 tail** | 单块 504B 期望约 273 个接受，**方差大**；可能 `<256`（需续 squeeze）或 `>256`（只取流上前 256 个） |
| **合法 0** | `d=0` 且 `<q` 是有效系数；不能用「提非零」代替「去掉拒绝标记」 |

### 13.2 方案 A：`Compares` 路线（`PHASE_A_VEC_REJ_PLAN` 既定）

`Compares(<q)` → mask → LUT/Gather **compact** → 写 `â`。SIM 上 `Compares` 曾挂死，v4a 退回标量 compare。

### 13.3 方案 B：`Mins` + mod（leaky-ReLU 类比，用户提议）

**lane 掩码**（与 Compare 数学等价，若 mod 边界正确）：

```text
tmp = Min(d, q)     // d<q → d；d≥q → q
out = mod(tmp, q)   // d<q → d；d≥q → 0
```

**`+q` / `2q` 编码**（为 compact 阶段区分合法 0 与空洞，可选）：

```text
d' = d + q
tmp = Min(d', 2q);  survivor = mod(tmp, 2q)   // 合法 d 编码为 d+q（含 d=0→q）；拒绝为 0
compact 后再统一 Sub(q) 还原 â∈[0,q)
```

**工程价值**：若 `Mins` + 既有 `mod_q_barrett_vec` / `wrap_mod` 在 SIM 比 `Compares` 稳，可作 A-v5 **掩码原语**单独 POC；**不替代** compact。

### 13.4 用户迭代方案（管线形状）

1. 对足够长 `d1[]`、`d2[]`：`Min(d,q)`，去掉 `==q`（拒绝标记）
2. **交错**：`d1[0], d2[0], d1[1], d2[1], …`（**禁止**先整段 d1 再整段 d2）
3. **紧凑靠前**排列
4. **取流上前 256** 个有效 cand → `â[256]`

**已对齐**：顺序、合法 0（筛 `==q` 而非非零）、掩码语义。

### 13.5 空间换时间：批量全算 + 截 256 与规范等价（同日追加，用户澄清）

规范表述为边扫边填、`j=256` 停；工程上可采用 **多生成字节、全量掩码、交错、compact、取前 256** 的批量路径。

| 点 | 结论 |
|----|------|
| **输出等价** | 第 256 个接受元之后的 cand **不改变** compact 序列前缀；与规范 **â 输出一致** |
| **`j<N` 隐式满足** | 同 pair 内 `d1` 若已是第 256 个接受，紧后 `d2` 为第 257 个，**截断 256 自然丢弃** |
| **规范 `j` 的角色** | 更多是 **省算力 / 少 squeeze 的早停**；非说批量路径语义错误 |
| **仍需** | 交错顺序对、缓冲够长（单块可能 `<256` 要 tail）、截断语义是「前 256 个**接受**」 |
| **真瓶颈** | **向量 compact** 与 UB 数据面；不是 Compare vs Min，也不是能否批量 |

### 13.6 AscendC 向量哲学（同日追加）

用户观点：**不存在太多「白算」**——AscendC 以 repeat/block 为基单位，不足 repeat 仍按一整块算；向量单元擅长宽载，**算力没占满才是浪费**。批量 rej 与「多算后面用不到的 cand」在向量语义下可接受；应优先 **64/128 lane 满占**（padding / 分 tile），而非为少算几个哑元 lane 退回标量早停。

### 13.7 推荐 POC 顺序（rej 段，未开工）

1. **d1/d2 交错** ROM `Gather`（168 pair → 336 stream）对拍 golden
2. 单 block：`Min+mod` 掩码 vs golden 逐 lane
3. 交错 stream + compact + **取前 256** vs golden `â[256]`（可先标量 compact）
4. 边界：`j=255` 同 pair（d1、d2 均 `<q`）；需 tail squeeze 的 seed
5. 64-pair tile 上向量 compact（SIM tick）；掩码 `Mins` 或 `Compares` 二选一

### 13.8 与 d12 / Phase A 关系

| 项 | 状态 |
|----|------|
| d1/d2 unpack（line 6–7） | d12 POC **CPU/SIM PASS**（shake_xof + GM 解交织 + 向量 d1/d2） |
| rej+compact（line 8–15） | **未实现**；形状接近 x86 `rej_uniform_avx2`，须 **UB 驻留、少 GM 往返** |
| 优先级 | 恢复 **715k 档** scalar rej 基线、**batch tail XOF** 与 rej 向量可并行；Host 上 rej ~5%，CAModel 上访问模式可放大 |

**定稿链**：讨论过程本文；原理沉淀待写入 `docs/notes/F203-Alg7-PhaseA-向量化技术总结.md` §rej Min+mod（下一版）。

---

## 14. d1/d2 交错连接：向量实现共识（同日追加）

**背景**：批量 rej 管线需 `stream[336]=d1[i],d2[i],…`；d12 POC 当前输出**平面** `d1[168]`、`d2[168]`（UB 内 `d1Local`/`d2Local` 已存在，不必经 GM 再读）。

### 14.1 几何

| 量 | 值 |
|----|-----|
| `d1` / `d2` | 各 168 int32 |
| 交错 `stream` | 336 int32 |
| repeat 对齐 | 336 非 64/128 整数倍；建议 **64 pair = 128 lane** 分 tile，或 pad 至 **384（128×3）** 尾部哑元 |

### 14.2 方案排序（共识）

| 优先级 | 方案 | 要点 |
|--------|------|------|
| **P0** | Alg.11 式 **`interleave_pairs_datacopy` + ROM `Gather`** | `DataCopy(d1→t1, d2→t2)`，`scratch=t1‖t2`，一次 `Gather` 出 `2n` 交错流；参照 `multiply_ntts_vec.hpp` `interleave_pairs_datacopy`；为 **n=168** 单独生成 `gAlg7InterleaveReorderByteGm[336]`（勿复用 n=128 Alg.11 表） |
| **P1** | **unpack 直写交错流** | `ComputeD12Vec` 后不落平面 d1/d2，按 tile zip 进 `stream[]`；省一整遍 interleave（调试对拍可保留平面 GM 输出） |
| **P2** | **与 rej 融合** | 每 64-pair tile：`Min(d1)`/`Min(d2)` → zip → tile compact → 追加 `â`（满 256 停）；不必物化全长 `stream[336]` |

### 14.3 反模式

- 标量 `aos[2*i]=d1[i]` 主路径（SIM 极慢）
- 每轮 `CreateVecIndex` 现算索引（168 固定长应用 **Init ROM 一次**）
- 平面 d1/d2 写 GM 再读回交错（多一轮 MTE；对齐 A-v4 搬运教训）
- 强行复用 Alg.11 `n=128` interleave ROM

### 14.4 rej 管线 POC 顺序（含交错）

1. ROM interleave 168 对 → golden 交错流对拍
2. tile 接 `Min+mod` 掩码
3. compact + 取前 256 → golden `â[256]`

**参考实现**：`multiply_ntts_vec.hpp` `interleave_pairs_datacopy`；ROM 模式同 `alg11_rom_tables.cpp`。

### 14.5 可行性判断与实现方案（同日定稿）

| 维度 | 结论 |
|------|------|
| 功能 | **可行**（批量+截 256 与规范等价） |
| 性能 | **待证**；瓶颈在 **向量 compact**，非 `Min`/交错 |
| 落地 | [`INTEGRATION_PLAN.md`](../../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/INTEGRATION_PLAN.md)：Gate R0–R5、UB 布局、`gen_alg7_interleave_rom.py` |

**定稿链**：讨论过程本文；单 poly 实现见上；原理沉淀待 `docs/notes/F203-Alg7-PhaseA-向量化技术总结.md` §rej（下一版）。

---

## 15. shake_xof_kernel LocalTensor I/O（2026-06-24）

**决策**：设备侧 SHAKE I/O 统一 **LocalTensor（UB）**；**删除** `RunShakeGeneralGmBridge`。集成用例单 TPipe 直连；toy 用 `shake_general_gm_io.hpp` 仅对拍。

| 项 | 内容 |
|----|------|
| **alg7** | ✅ 单 TPipe：`FillSampleSeedUb` → `RunShake128SampleNttUb` → `Deinterleave3x168FromUb`；kernel 去掉 shake_x/len GM |
| **toy** | ✅ `*_toy_ub.hpp` 全 UB 自检；无 GM x/y；`auto_gen/toy_active_case.h` |
| **SIM tick** | alg7 scalar/vec ~**76939** / ~**76928**（较 GM 直写 +~17k，待 profile） |
| **待迁** | `pass-fix-f203-alg13-lines8-15-se-k4` PRF |

### 15.1 Phase 1：共享库块 I/O（2026-06-24）

**改动**：`Init` 增 `staging32`；`XorBlock32` / `StoreBlock32` 以 **uint64 块**（4×/32B）替代逐字节 Get/Set；尾 &lt;32B 仍标量。

**SIM 约束**：首版曾用 `DataCopy(staging→y)`，CaModel 上 toy 自检 FAIL（`GetValue` 读 y 不可见）；改为 `y64.SetValue` 后 CPU+SIM 均 PASS。吸收侧亦不用 `DataCopy(x→staging)`，直接 `x64.GetValue`。

| 用例 | CPU | SIM tick（Phase 1 后） |
|------|-----|------------------------|
| shake128 toy | ✓ | ~**10548**（前 ~12285） |
| shake256 toy | ✓ | ~**10923** |
| alg7 scalar/vec rej | ✓ | ~**67967** / ~**68024** |

**下一步（Phase 2）**：`FillSampleSeedUb` 块写；默认关 `DumpXofUbToGm`；`Deinterleave` Gather/ROM。

### 15.2 Phase 2：alg7 胶水（2026-06-24）

| 项 | 内容 |
|----|------|
| `FillSampleSeedUb` | ρ 4×uint64 + j‖i 单 uint32（替代 34×SetValue） |
| `F203_ALG7_DUMP_XOF` | 默认 **0**；`verify_result.py` 跳过 xof；调试 `F203_ALG7_DUMP_XOF=1` |
| `DumpXofUbToGm` | 480B DataCopy + 24B 标量尾（GM 仅 504B，不可写 512B） |
| SIM tick | scalar/vec ~**55738** / ~**55735**（较 Phase1 ~67967，主要因去掉 dump 路径） |

**下一步（Phase 3）**：`Deinterleave3x168FromUb` Gather+ROM；或 squeeze 直写 c0/c1/c2。

### 15.3 Phase 3：解交织 Gather+ROM（2026-06-24）

| 项 | 内容 |
|----|------|
| ROM | `scripts/gen_alg7_deinterleave_rom.py` → `f203_alg7_deinterleave_rom.h`（`4×(3k+lane)` 字节偏移，int32 Gather） |
| 路径 | `expanded[504]` 零扩展 pack → 3×`Gather(c0/c1/c2)`；**`F203_ALG7_D12_GATHER=0` 生产默认** |
| 约束 | uint8 直 Gather 在 CPU/SIM 不可用；须 int32 expanded + 4 对齐索引 |
| SIM tick | ~**73105**（负优化）；**默认已回退 Phase2 标量 ~55738** |
| 对照 | `F203_ALG7_D12_GATHER=0` 标量 `GetValue`（`bash run.sh` 默认） |

**未做**：squeeze 直写 c0/c1/c2（消 xof_ub 物化）；pack 改向量/DataCopy。

### 15.4 shake 优化推广（2026-06-24）

- 新增 **`library/shared/shake_xof_kernel/shake_ub_helpers.hpp`**：`RunKernelShakeGeneralUb` + `staging32` + 块写消息
- **toy** / **alg7** / **`pass-fix-f203-alg13-lines8-15-se-k4` PRF** 已统一 UB 接线；禁止旧 GM `Init(x_gm,…)`
- **alg7 解交织**：生产默认 Phase2 标量；`F203_ALG7_D12_GATHER=1` 仅实验
### 15.5 examples 边界澄清（2026-06-24）

- **`exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4`**：**不**做 AscendC 实时生成 $\mathbf{s}$/$\mathbf{e}$；设备只读 Host/Python `input/src.bin`（行 16–20 MIX 核）
- 设备预采样 + `shake_ub_helpers` 归属 **`../../ascendc-tests/`**（[`pass-fix-f203-alg13-lines8-15-se-k4`](../../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/) / chain_ntt17），与 exp 解耦
- 已撤回误加的 customspec §Phase2 双 launch 与 `f203_device_presample_ub.hpp`

---

## 16. XOF 固定 672B（2026-06-24，用户拍板）

**决策**：`pass-fix-f203-alg7-sample-ntt-k4` **不做** Kyber 式 `while (ctr<256) squeeze(168)`；一次 squeeze **672B = 504+168**（同 absorb 续流语义）。

| 项 | 内容 |
|----|------|
| 动机 | 向量管线固定块长、无二次 XOF 分支；多 168B 与多算 cand 可接受（§13.5–§13.6） |
| 几何 | `kCandPairs=224`，`stream=448`，d1/d2 GM 各 **896B**；`scripts/alg7_geom.py` ↔ `f203_alg7_layout.h` |
| Golden | `shake128(seed).digest(672)`；`test_multi_seed` 28 组 PASS |
| Gate R4 | 由「lazy tail」改为 **固定预 squeeze** ✅ |
| 后继 | R5 向量 compact 按 **64+64+64+32** pair tile 推进 |

**定稿**：[`INTEGRATION_PLAN.md`](../../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/INTEGRATION_PLAN.md) §1.4、§3.5。

---

## 17. 672B tick 代价与「原型保险 / 后续可收紧」（2026-06-24）

**实测**（`pass-fix-f203-alg7-sample-ntt-k4`，scalar rej，blockDim=1）：

| XOF | SIM tick |
|-----|----------|
| 504B | ~55738 |
| **672B** | **~63213–63265**（**+~13%**） |

**归因**：多 squeeze **168B**（1×SHAKE128 rate）→ Keccak 路径变长；**不是**「为避免 tail while 而净赚 tick」。504 对绝大多数种子已够，672 是原型阶段 **故意多付** 的保险。

**用户结论（纪要）**

| 点 | 内容 |
|----|------|
| 当前 | **实现原型** → 672B **可接受**（固定几何、少分支） |
| 后续优化 | 可 **退回 504B + lazy tail squeeze(168)**；母探针上甚至可再讨论 **仅首批、tail 按需**（与向量固定块长 trade-off） |
| 记录 | tick 对照写入探针 `STATUS.md`、`INTEGRATION_PLAN.md` §1.5 |

**定稿链**：讨论本文；原理待 `docs/notes/F203-Alg7-PhaseA-向量化技术总结.md` 下一版 §XOF 定长。

---

## 18. rej 剔除双方案 POC（2026-06-23）

**用户要求**：固化实现方案（rej 剔除热路径 **禁止 for+GetValue**）；**方案 A** `Compares(LT)+Select` 与 **方案 B** `Mins` 分别实现并 SIM 对比。

| 方案 | `F203_ALG7_REJ_IMPL` | 实现 | CPU | SIM tick（672B，blockDim=1） |
|------|----------------------|------|-----|------------------------------|
| scalar | **0** | 标量 `GetValue` rej | ✓ | **63256** |
| **vec_mins（默认）** | **1** | `Mins(d,q,224)` ×2 → Gather → 标量 compact | ✓ | **63222** |
| vec_mask | **2** | `Compares+Select` 128+96 两 tile | ✓（CPU 孪生 dispatch→Mins） | **63249** |

**结论**

| 项 | 内容 |
|----|------|
| 语义 | 两向量方案 `a_hat` 与 scalar golden **max_abs_diff=0** |
| tick | **vec_mins ≈ vec_mask ≈ scalar**（差 <0.1%）；**Mins 略优**于 Compares+Select |
| 瓶颈 | compact 仍标量 `GetValue`；剔除段已向量化后 **整体 tick 仍被 XOF/compact 主导** |
| 定稿 | `f203_alg7_config.h` `F203_ALG7_REJ_IMPL`；`INTEGRATION_PLAN.md` §3.2 |

**遗留**：R5 向量 compact **暂停**（见 note §5）；生产默认 **`F203_ALG7_REJ_IMPL=1`**，标量 `0` 仅回归。

---

## Alg.14 Encrypt G3 修正（stable-fips203-mlkem-pke-encrypt-k4）

**用户意见（2026-06-23）**：事实须**原封不动**记录；**在推进 G4/G5 之前先修正 G3**；SIM 耗时长，改前想清楚、少做无效实验。

### 已记录事实（全文见探针 [`G3_SIM_AUDIT.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/G3_SIM_AUDIT.md)）

1. CPU 始终 1× `f203_encrypt_g3_linear`；修正前 SIM 为 2× `at_r` + Host **fake-Â**（`t_hat` 填矩阵列 0，row0 当 `tr_hat`）。
2. 独立 `t_dot_r` SIM 向量路径曾全零；同 session 双 launch 写回异常 → 多 ACL session + fake-Â workaround。
3. `t_hat` 由 Host `decode_t_hat.py` staging；G3 设备未解码 `ek`。
4. 对拍非抄 golden，但 **CPU/SIM 路径不对等**；历史 G4 SIM PASS ≠ G3 核 SIM 已验收。

### 终态（2026-06-29 验收）

| 模式 | 结果 |
|------|------|
| CPU G3 | **PASS** `g3_linear`；u_hat/tr_hat max=0 |
| SIM G3 | **PASS** max=0；wall **327s** |

**SIM 路径**：`at_r(真 a_hat)→u_hat` + `at_r(t̂ 列 0)→row0=tr_hat`（与 `t_dot_r` 数学等价）。`t_dot_r` / `g3_linear` 五参 launch 在 SIM 返回 **507000**（审计 §7–§8）。

**状态**：**G3 修正完成**；G4 全链 SIM 可择机复验；G5 未动。
