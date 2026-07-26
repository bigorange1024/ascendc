# FIPS 203 ML-KEM-768 Alg.13 K-PKE.KeyGen — baseline-registry（草稿）

**主题**：Alg.13 K-PKE.KeyGen（k=3）交付/预研侧 golden / KAT 计算块登记  
**适用（目标）**：`examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-keygen-k3/`（E13 incubating）
**参数卡**：[fips203-mlkem768-parameter-card.md](fips203-mlkem768-parameter-card.md)  
**状态**：E13 预研已验证（CPU + `SIM_DIRECT=1` sim）；liboqs KAT / NPU 未跑

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `seed_d.bin` | 4 B | uint32 LE；默认 host SHA3 |
| 输出/golden | `ek_pke.bin` | 1184 B | ByteEncode12(t̂)‖ρ |
| 输出/golden | `dk_pke.bin` | 1152 B | ByteEncode12(ŝ) |

中间量默认 **禁止**落盘（调试 dump 标非默认）。

---

## 2. 计算块登记

| 计算块 | 用途 | 已验证来源 | 状态 |
|--------|------|------------|------|
| liboqs `ML-KEM-768` oracle | 后续 KAT / 交叉验证 | `OQS_KEM_ml_kem_768_*`；仓库现有 `scripts/liboqs_kem_fixture.py` 仍为 1024 尺寸 | **未跑**（非 E13 本轮门禁） |
| host golden `build_full_keygen` | E13 `ek_pke.bin` / `dk_pke.bin` I/O oracle | `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-keygen-k3/scripts/gen_data.py` + `scripts/verify_production.py` | **已验证**：`KEYGEN_GOLDEN_ONLY=1 python3 scripts/gen_data.py && python3 scripts/verify_production.py` PASS |
| 静态 NTT LUT | limb 编码 | 本 exp `scripts/compute/gen_data.py` + vendored `thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h` | **已验证**：E13 CPU/SIM `ek_pke`/`dk_pke` max=0 |
| Derand `d` | host/device 同式 | 参数卡 §4 域分离串；`exp-mlkem-f203-2s1e-k3:SEED_D=` | **已验证**：默认 `SEED_D=20260619` 下 E13 CPU/SIM PASS |
| NTT / CBD / ByteEncode12 核心 | 设备算法 | E13/D13 k3 AscendC 自包含副本（Â[9]、polyvec6、Inner 2+1、ByteEncode12 `3×384`） | **已验证**：CPU PASS；SIM tick **373429** |

---

## 3. 明确禁止

- 用 ML-KEM-1024 fixture 改长度冒充 768。
- 在 golden 路径重写 NTT/CBD/ByteEncode/Compress 核心。
- 缺项时继续写基准计算（须停下补本表）。

---

## 4. 维护

E13 已登记；后续若补 liboqs KAT / NPU，应在本表追加具体命令、fixture 与提交。本表当前仅支撑 768 E13 incubating 预研，不表示 stable-768 可晋级。
