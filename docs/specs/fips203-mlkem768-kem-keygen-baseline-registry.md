# FIPS 203 ML-KEM-768 Alg.19 ML-KEM.KeyGen — baseline-registry（草稿）

**主题**：Alg.19 ML-KEM.KeyGen（k=3）交付/预研侧 golden / KAT 计算块登记  
**适用（目标）**：`examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-keygen-k3/`  
**参数卡**：[fips203-mlkem768-parameter-card.md](fips203-mlkem768-parameter-card.md)  
**状态**：预研 E19 **CPU+SIM 已验证**（2026-07-26）· 非 stable 交付登记

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `seed_d.bin` | 4 B | uint32 LE |
| 输出/golden | `ek_kem.bin` | 1184 B | = ek_PKE |
| 输出/golden | `dk_kem.bin` | 2400 B | dk_pke‖ek‖H(ek)‖z |

中间量默认 **禁止**落盘（调试 dump 标非默认）。

---

## 2. 计算块登记

| 计算块 | 用途 | 已验证来源 | 状态 |
|--------|------|------------|------|
| liboqs `ML-KEM-768` oracle | 可选 KAT / 交付级外部 oracle | `OQS_KEM_ml_kem_768_*`；仓库现有 `scripts/liboqs_kem_fixture.py` 仍为 1024 尺寸 | **未跑**（后续 P3 补；E19 不依赖该项 claim） |
| E13 PKE KeyGen oracle | 生成 `ek_pke` / `dk_pke` golden，再拼 KEM 展开布局 | `exp-fips203-mlkem-kem-keygen-k3/scripts/keygen_golden.py`（vendored 自 E13 已绿 oracle） | **已验证**：E19 CPU/SIM `ek_kem`/`dk_kem` max=0 |
| 静态 NTT LUT | limb 编码 | `scripts/compute/gen_data.py` + `thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h`（本目录实体文件） | **已验证**：E19 CPU/SIM PASS；与 E13/D19 同 LUT 形态 |
| Derand `d`/`z` | host/device 同式 | 参数卡 §4 域分离串；`d`: `exp-mlkem-f203-2s1e-k3:SEED_D=`，`z`: `exp-mlkem-f203-kem-k3:SEED_Z=` | **已验证**：`SEED_D=20260619` 下 CPU/SIM max=0 |
| `H(ek)` | Alg.16 tail | Host golden `hashlib.sha3_256(ek)`；device `library/shared/keccak_f1600_kernel` `Sha3OneShot` | **已验证**：E19 CPU/SIM `dk_kem` max=0 |
| NTT / CBD / ByteEncode12 核心 | 设备算法 | E13/D13 k3 AscendC 自包含副本（Â[9]、polyvec6、Inner 2+1、ByteEncode12 `3×384`） | **已验证**：E19 CPU/SIM PASS；golden 只作 I/O oracle，禁止将 Python 过程视作 AscendC 规格 |
| AscendC-only KEM roundtrip glue | E19 输出作为 E20/E21 输入 | `scripts/exp_kem768_liboqs_roundtrip.sh`（当前不使用 liboqs-768；见脚本头注释） | **已验证**：CPU×1 + SIM×1 PASS；`ek/dk` 尺寸 1184/2400B；accept/reject 全链通过 |

---

## 3. 明确禁止

- 用 ML-KEM-1024 fixture 改长度冒充 768。
- 在 golden 路径重写 NTT/CBD/ByteEncode/Compress 核心。
- 缺项时继续写基准计算（须停下补本表）。

---

## 4. 维护

每通过一块 → 把「未验证」改为具体路径与提交；进 INDEX 时同步日期。本表当前仅支撑 768 E19 incubating 预研与 AscendC-only roundtrip，不表示 stable-768 可晋级。
