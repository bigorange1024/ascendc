# INTEGRATION_PLAN — pass-fix-f203-alg19-kem-keygen-device-k4

**定位**：`ascendc-tests/` **Alg.19 `ML-KEM.KeyGen()` 设备主线**（**ml_kem_1024 / k=4**）。在 **不增加相对 stable PKE KeyGen 的 kernel launch 次数、不引入额外 Host↔Device 往返搬运** 的前提下，把 **Alg.19 → Alg.16 → Alg.13** 串成 **2 launch** 全链。

**基线对照**（只读，禁止 rsync `vendor/`）：

| 探针 | 角色 |
|------|------|
| 历史 correctness oracle | **已冻结**（2026-07-20）— 只读 [`FROZEN.md`](../frozen/frozen-fix-f203-alg19-kem-keygen-correctness-k4/FROZEN.md)；**禁止**抄码 / 跑 CI |
| [`stable-fips203-mlkem-pke-keygen-k4`](../../examples/stable/stable-fips203-mlkem-pke-keygen-k4/) | **Alg.13 PKE KeyGen** 权威实现（2 launch，SIM ~542k tick） |

**定稿原理**：[`docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md`](../../docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md)

**自包含**：[`SELF_CONTAINED.md`](SELF_CONTAINED.md) · [`用例自包含与设备全链约束.md`](../../docs/engineering/用例自包含与设备全链约束.md)

**用户锁定（2026-07-10）**：

| 项 | 约定 |
|----|------|
| **PKE 来源** | **编译期**引用 stable KeyGen 的 `prep/` + `compute/`（无 `vendor/`、无 `vendor_sync`）；领导若日后要求单目录自包含再 copy，本方案接口不变 |
| **Launch 预算** | **= stable PKE：2 次**（`prep` \| `mmad`）；**禁止** correctness 式第 3 launch `f203_kem_kg_finish` |
| **搬运预算** | 生产路径仅 **最终** D2H `ek_kem`/`dk_kem`；Alg.16 尾段在 Launch-2 末尾 **UB/GM 就地**完成，禁止为 KEM 尾段单独 `aclrtSynchronizeStream` + 再 launch |
| **I/O** | 与 correctness 相同：`SEED_D=20260619`；`ek_kem` 1568B · `dk_kem` 3168B（liboqs 展开） |
| **随机性** | `d`/`z` device UB，不导出；`KEM_KG_EXT_SEED` 旁路 A 保留（test-only） |

---

## 1. FIPS 代数链：Alg.19 → Alg.16 → Alg.13

### 1.1 对外：Algorithm 19

```text
d ←$ B^32          // Launch-1 prep 内 UB：DerandFromSeedD(seed_d)
z ←$ B^32          // Launch-2 mmad 尾 UB：DerandZFromSeedD(seed_d)
(ek, dk) ← ML-KEM.KeyGen_internal(d, z)   // Alg.16
```

### 1.2 内部：Algorithm 16 `KeyGen_internal`

```text
(ek_PKE, dk_PKE) ← K-PKE.KeyGen(d)     // Alg.13，Launch-1+2
ek  ← ek_PKE
dk  ← dk_PKE || H(ek) || z
```

**liboqs 展开 `dk_kem`（本探针 I/O 锁定）**：

```text
dk_kem = dk_pke (1536) || ek (1568) || H(ek) (32) || z (32)   → 3168B
ek_kem = ek_PKE (1568)                                        // 与 ek 字节相同
```

### 1.3 内层：Algorithm 13（stable 已验收）

| FIPS 行 | 设备段 | Launch |
|---------|--------|--------|
| 3–15 | `f203_keygen_prep`：Â、ρ/σ、ŝ/ê CBD | **L1** |
| 16–20 | `mmad_custom`：NTT、内积、ByteEncode₁₂ | **L2** |
| 21 | `FuseEkPke`：ek_PKE = ek_polyvec ‖ ρ | **L2 末尾 AIV0** |

**Alg.16 增量（本探针新增，仍在 L2 末尾）**：

| 步骤 | 操作 | 数据位置 |
|------|------|----------|
| L2-T1 | `DerandZFromSeedD(seed_d, z)` | UB only |
| L2-T2 | `H(ek) = SHA3-256(ek_PKE)` | ek 从 `ek_pke_gm` 读入 UB → `h[32]` UB |
| L2-T3 | `ek_kem ← ek_PKE` | **GM 别名**，见 §3.2 |
| L2-T4 | `dk_kem ← dk_pke ‖ ek ‖ H ‖ z` | 顺序写 `dk_kem_gm[3168]` |

> **关键**：Alg.13 与 Alg.16 在 **同一次 `mmad_custom` launch** 内首尾相接；`d` 在 L1 已消费完毕，L2 尾段只再读 **4B `seed_d_gm`** 派生 `z`，不读不写 `d` 本体。

---

## 2. Launch 拓扑（定案：2 launch）

```text
aclInit / CreateStream
  │
  ├─ Launch-1: f203_keygen_prep          // = stable L1，Alg.13 行 3–15
  │            seed_d_gm[4] → UB: d → ρ/σ → …
  │            写 GM: a_hat, src, rho, …
  │            （无 stream sync 要求高于 stable；与 stable main 一致）
  │
  ├─ Launch-2: mmad_custom             // = stable L2 + Alg.16 内嵌尾
  │            读: src, a_hat, rho, ws(LUT), seed_d_gm
  │            写: ek_pke_gm[1568], dk_pke_gm[1536]  // Alg.13 行 16–21
  │            尾（AIV0，F203_KEM_KEYGEN_TAIL=1）:
  │              DerandZ → H(ek) → 拼 dk_kem_gm[3168]
  │              ek_kem 与 ek_pke_gm 同址（零拷贝）
  │
  ├─ aclrtSynchronizeStream            // 仅一次，Launch-2 后
  ├─ D2H: ek_kem.bin, dk_kem.bin
  └─ aclFinalize
```

### 2.1 相对 correctness-k4 的差异

| 项 | correctness（3 launch） | **device-k4（本方案）** |
|----|-------------------------|------------------------|
| Launch 数 | prep \| mmad \| **kem_finish** | prep \| **mmad+tail** |
| KEM 尾段符号 | 独立 `f203_kem_kg_finish` | **`KemKgTailFused`** 内嵌于 mmad |
| 中间 sync | L2 后 + L3 后 | **仅 L2 后**（与 stable 相同） |
| SIM tick 目标 | ~742k | **~stable+α**（α = 尾段 SHA3+拼接，预期 ≪ 200k launch 开销） |

### 2.2 禁止项

- 子进程调用 stable `run.sh` 再拼 KEM
- Host `tiny_sha3` / liboqs 参与生产路径
- 为 H(ek) 或 z 单独增加 Launch-3
- Launch-2 与 Launch-3 之间对 `ek_pke`/`dk_pke` 做 D2H→H2D

---

## 3. GM 布局与零额外搬运

### 3.1 Host 分配（在 stable `main_keygen` 基础上扩展）

| GM 缓冲 | 尺寸 | 用途 |
|---------|------|------|
| `seed_d_gm` | 4B（旁路 A：64B） | L1+L2 尾读 |
| `a_hat` … `rho` … | 同 stable | L1 写 / L2 读 |
| `ws` | LUT + workspace | L2 |
| `ek_pke_gm` | 1568 | L2 写；**= `ek_kem_gm` 别名** |
| `dk_pke_gm`（sk） | 1536 | L2 写；L2 尾读 |
| `dk_kem_gm` | **3168** | **仅 L2 尾写**（新增唯一 KEM 专用 GM） |

**不新增**：`h_gm`、`z_gm`、`ek_kem` 独立缓冲（H 与 z 全程 UB）。

### 3.2 `ek_kem` 零拷贝

`ek_kem` 与 `ek_PKE` 字节恒等（Alg.16 行 2）。Host 侧：

```cpp
uint8_t *ek_pke_gm = …;
uint8_t *ek_kem_gm = ek_pke_gm;   // 同指针，D2H 一次即可
```

设备尾段 **省略** `GmMemcpyU8(ek_kem, ek_pke)`（correctness 有拷贝，本方案删除）。

### 3.3 `dk_kem` 就地拼接

在 `ek_pke_gm`、`dk_pke_gm` 已就绪的 **同一 launch** 内，AIV0 顺序写 `dk_kem_gm`：

```text
[0:1536)   ← dk_pke_gm
[1536:3104) ← ek_pke_gm（读，不写回 ek 缓冲）
[3104:3136) ← H(ek) UB
[3136:3168) ← z UB
```

全程 GM→UB / UB→GM 仅为拼接所需，**无跨 launch 搬运**。

### 3.4 `seed_d_gm` 贯穿

- L1：`f203_keygen_prep` 已读 `seed_d` 派生 `d`（stable 行为）
- L2 尾：再读同一 `seed_d_gm` 调 `DerandZFromSeedD`
- **不**为 z 单独 H2D；旁路 A 时 `seed_d_gm` 扩 64B，尾段读 `[32:64]`（与 correctness 契约一致）

---

## 4. 代码组织（无 vendor）

### 4.1 目录结构

```text
pass-fix-f203-alg19-kem-keygen-device-k4/
├── INTEGRATION_PLAN.md          # 本文件
├── STATUS.md
├── SELF_CONTAINED.md
├── run.sh
├── main_kem_keygen.cpp          # ≈ stable main + dk_kem GM + 2 launch
├── f203_kem_kg_layout.h         # 从 correctness 迁（I/O 常量）
├── kem/
│   ├── f203_kem_kg_derand_ub.hpp    # DerandZFromSeedD（从 correctness 迁）
│   ├── f203_kem_kg_tail_fuse.hpp    # KemKgTailFused（从 finish.hpp 改：无 ek 拷贝）
│   └── f203_keygen_prep_extseed.hpp # 旁路 A prep（从 correctness 迁）
├── compute/
│   └── mmad_custom_kem.cpp        # stable mmad + 尾钩子（见 §4.3）
├── cmake/keygen/CMakeLists.txt
└── scripts/
    ├── gen_data.py                # 可指向 stable scripts 或薄封装
    └── prepare_production_input.py
```

**不创建** `vendor/`。`prep/` 软链或 CMake `STABLE_ROOT` 直接编 stable 源。

### 4.2 CMake 要点

```cmake
set(STABLE_KEYGEN_ROOT "${REPO_ROOT}/examples/stable/stable-fips203-mlkem-pke-keygen-k4")
set(TEST_ROOT "${STABLE_KEYGEN_ROOT}")   # prep + cpu/npu_lib_keygen.cmake

# KERNEL_FILES：prep 入口 + mmad_custom_kem.cpp（非 stable 原版 mmad）
set(KERNEL_FILES
    ${_KEYGEN_PREP_ENTRY}
    ${CMAKE_CURRENT_LIST_DIR}/../../compute/mmad_custom_kem.cpp
)

# 仅 device-k4 打开：
set(F203_KEM_KEYGEN_TAIL "1")
ascendc_compile_definitions(... F203_KEM_KEYGEN_TAIL=1 F203_KEYGEN_EK_PKE=1 ...)
```

`include(${STABLE_KEYGEN_ROOT}/cmake/cpu_lib_keygen.cmake)` 时编译 **stable** `compute/mmad_custom.cpp`，并设 `F203_KEM_KEYGEN_TAIL=1`。

### 4.3 `mmad_custom` 接线（**已采纳 A**，2026-07-10）

**方案 A — stable 可选宏（长期维护）** ✅

在 stable `compute/mmad_custom.cpp` 的 `FuseEkPke` 之后增加：

```cpp
#if F203_KEM_KEYGEN_TAIL >= 1
    if (!AIC && subBlockID == 0 && runEncode) {
        F203KemKg::KemKgTailFused(seed_d_gm, ek_pke_gm, sk_out, dk_kem_gm);
    }
#endif
```

- stable 默认 `F203_KEM_KEYGEN_TAIL=0`，PKE 行为不变
- device-k4 CMake 置 `1`，编译 stable `mmad_custom.cpp`（**无 fork**），并追加 GM 实参 `seed_d_gm`、`dk_kem_gm`
- ROM：`scripts/prep/gen_alg7_*.py` 写入 device 本地 `prep/alg7/`（`f203_alg7_layout.h` 仍自 stable include）

**备选 B — 探针内 fork** — 已废弃（首期 B 已删除）

### 4.4 `KemKgTailFused` 逻辑（自 correctness `KemKgFinishImpl` 改写）

```cpp
// kem/f203_kem_kg_tail_fuse.hpp
__aicore__ inline void KemKgTailFused(
    __gm__ uint8_t *seed_d_gm,
    __gm__ uint8_t *ek_pke_gm,    // ek_kem 同址，不拷贝
    __gm__ uint8_t *dk_pke_gm,
    __gm__ uint8_t *dk_kem_gm)
{
    // 1) z ∈ UB（DerandZFromSeedD 或 KEM_KG_EXT_SEED 旁路）
    // 2) H(ek) ∈ UB（Sha3OneShot，ek 从 ek_pke_gm 读入 UB）
    // 3) 拼 dk_kem_gm（四段顺序写）
    // 不写 ek_kem 独立缓冲
}
```

**同步**：尾段在 AIV0 完成；沿用 `FuseEkPke` 后的 `KYBER_PIPE_ALL()`，不新增跨核握手。

### 4.5 P1 定案保留理由与 SHA3 替换（2026-07-10）

**定案**：保留方案 A（stable 宏 + 本地 ROM），废弃 mmad fork。

| 收益 | 说明 |
|------|------|
| 无 mmad fork | stable NTT/Alg.11/行21 演进时不必手工 merge `mmad_custom_kem.cpp` |
| stable 树不被 ROM 污染 | `run.sh` 将 `gen_alg7_*.py` 输出写入 **device** `prep/alg7/` |
| PKE 零回归 | `F203_KEM_KEYGEN_TAIL=0` 时 mmad 与纯 KeyGen 一致 |
| 尾段仍集中在 `kem/` | alg20/21 device 可复用「stable 主体 + kem 尾」模式 |

**代价**：SIM tick 较 fork 首期约 **+1.8%**（3 次复测均值 **713227**，极差 24）；I/O 与 correctness **cmp 一致**。工程收益大于 tick 差，故保留。

**SHA3 替换（日后第三方 AscendC 实现）** — 本探针 KEM 尾段仅 **SHA3-256** 两处：

| # | 文件 | 调用 | 输入 → 输出 |
|---|------|------|-------------|
| 1 | `kem/f203_kem_kg_derand_ub.hpp` | `Sha3OneShot(z, 32, msg, pos)` | 域分离串 → z[32] |
| 2 | `kem/f203_kem_kg_tail_fuse.hpp` | `Sha3OneShot(h, 32, ek_ub, 1568)` | ek_PKE → H(ek)[32] |

当前实现：`library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp`（`F203SeDeviceKeccak::Sha3OneShot`，mdlen=32/64）。

替换契约：

- API 须 `__aicore__`、一次性摘要（或等价 one-shot），**勿**改 `f203_kem_kg_layout.h` 偏移与 golden 域分离串；
- 可新建 `kem/f203_kem_sha3.hpp` 统一封装两处调用，再换第三方实现；
- stable prep 内 **SHA3-512**（ρ‖σ）走同库不同 mdlen，全链换库时 prep 与 kem 一并评估；
- 验收：`bash run.sh -r cpu/sim` + `cmp` correctness；可选 liboqs kat。

---

## 5. Host `main_kem_keygen.cpp`

基于 stable [`main_keygen.cpp`](../../examples/stable/stable-fips203-mlkem-pke-keygen-k4/main_keygen.cpp) 最小 diff：

| 变更 | 说明 |
|------|------|
| 分配 `dk_kem_gm[3168]` | 唯一新增 GM |
| `ek_kem_gm = ek_pke_gm` | 别名 |
| Launch-2 实参 | 追加 `seed_d_gm`、`dk_kem_gm`（KEM 构建） |
| D2H | `ek_kem.bin` ← `ek_pke_gm`；`dk_kem.bin` ← `dk_kem_gm` |
| 删除 | 第三次 `aclrtlaunch_f203_kem_kg_finish` |
| 日志 | `launches=2 (prep | compute+kem_tail)` |

`KEM_KG_EXT_SEED=1` 时：`seed` 缓冲 64B、prep 入口换 `f203_keygen_prep_entry_extseed.cpp`（与 correctness 相同）。

---

## 6. 分阶段 Gate

| Gate | 范围 | 验收 |
|------|------|------|
| **G0** | CMake + 2 launch 壳 | kernel 正常结束 |
| **G1** | L1+L2 的 PKE 段（`F203_KEM_KEYGEN_TAIL=0` 临时关尾） | `ek_pke`/`dk_pke` vs stable 同 `SEED_D` max=0 |
| **G2** | 开尾段，仅 `H(ek)`+`z` | vs correctness `gen_data` / host golden 中间量 |
| **G3** | 全链 `ek_kem`/`dk_kem` | vs correctness-k4 + liboqs max=0 |
| **G4** | 生产 `run.sh` 默认 | `KEM_KEYGEN_VERIFY=1` CPU+SIM PASS |
| **G5** | 旁路 A | `liboqs_kem_keygen_batch.sh` 11/11 |
| **G6** | 性能 | SIM tick ≤ stable + **50k**（尾段预算；未达则 profile，不阻塞 G3） |

**过渡**：G1 通过前可用宏关闭尾段；G3 通过后禁止默认关尾。

---

## 7. `run.sh`

| 项 | 约定 |
|----|------|
| 默认 | 同 correctness：`KEM_KEYGEN_VERIFY=1`，`SEED_D=20260619` |
| 无 `vendor_sync` | `SCRIPTS_PREP` 指向 stable `scripts/prep` 或本目录薄封装 |
| profile 隔离 | 保留 `prod` / `extseed` 双 profile（防构建污染） |
| SIM | `source camodel_sim_log.sh`；`KERNEL_COMPUTE_BUDGET_SEC` 默认 900 |

---

## 8. 验收命令

```bash
cd ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4

# G1：临时 F203_KEM_KEYGEN_TAIL=0 构建后，ek_pke/dk_pke vs stable
cd ../../examples/stable/stable-fips203-mlkem-pke-keygen-k4 && bash run.sh -r cpu -v Ascend910B4

# G3/G4
cd ../../ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# 与 correctness 对照
# 历史 correctness 已冻结；勿再对拍其 output（见 frozen/.../FROZEN.md）

# L2 liboqs（仓库根；device 验收通过后改 KEYGEN_DIR 默认）
bash scripts/liboqs_kem_vs_ascendc.sh -r cpu -v Ascend910B4
```

---

## 9. 风险与对策

| 风险 | 对策 |
|------|------|
| mmad 签名变更影响 stable | 宏默认 0；KEM 实参仅 `F203_KEM_KEYGEN_TAIL=1` 编译单元 |
| MIX AIV0 尾段与 Pipe 状态 | 尾段用独立小 `TPipe`（仿 `FuseEkPke`）；`KYBER_PIPE_ALL()` 后再进尾 |
| SIM func_key / session | 2 launch 与 stable 同型；G4 SIM 为硬门禁 |
| stable 漂移 | device 与 stable 同 commit 验收；INDEX 注明依赖 stable 版本 |
| 领导要求单目录自包含 | 将 `STABLE_KEYGEN_ROOT` 改为本目录 `pke_keygen/` 一次性 copy；**launch/GM 契约不变** |

---

## 10. 与下游探针

| 探针 | 消费 |
|------|------|
| [`pass-fix-f203-alg20-kem-encaps-device-k4`](../pass-fix-f203-alg20-kem-encaps-device-k4/) | `output/ek_kem.bin` |
| [`pass-fix-f203-alg21-kem-decaps-device-k4`](../pass-fix-f203-alg21-kem-decaps-device-k4/) | `output/dk_kem.bin`（交付） |
| [`pass-fix-f203-alg21-kem-decaps-device-ct-k4`](../pass-fix-f203-alg21-kem-decaps-device-ct-k4/) | `output/dk_kem.bin`（CT 专题） |

device-k4 **G3 PASS** 前，round-trip 脚本仍默认 **correctness-k4**。

---

## 11. 实施顺序（写码 checklist）

1. 迁 `f203_kem_kg_layout.h`、`kem/*`（`KemKgTailFused` 去 ek 拷贝）
2. CMake：`STABLE_KEYGEN_ROOT` + `mmad_custom_kem` 替换
3. `main_kem_keygen.cpp`：2 launch + `dk_kem_gm`
4. **G1** PKE 段对 stable
5. 接尾段 → **G3** 对 correctness
6. CPU+SIM **G4** + liboqs **G5**
7. 刷新 `STATUS.md`、`ascendc-tests/INDEX.md`、`qa/TODO.md` T19d
