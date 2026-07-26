# FIPS 203 ML-KEM-768 Alg.15 K-PKE.Decrypt — baseline-registry（草稿）

**主题**：Alg.15 K-PKE.Decrypt（k=3）交付/预研侧 golden / KAT 计算块登记  
**适用（目标）**：`examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-decrypt-k3/`（E15 incubating）
**参数卡**：[fips203-mlkem768-parameter-card.md](fips203-mlkem768-parameter-card.md)  
**状态**：E15 预研已验证（CPU + `SIM_DIRECT=1` sim）；PKE roundtrip / liboqs KAT / NPU 未跑

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `dk_pke.bin` | 1152 B | 私钥 |
| 输入 | `c.bin` | 1088 B | 密文 |
| 输出/golden | `m.bin` | 32 B | 明文 |

中间量默认 **禁止**落盘（调试 dump 标非默认）。

---

## 2. 计算块登记

| 计算块 | 用途 | 已验证来源 | 状态 |
|--------|------|------------|------|
| liboqs `ML-KEM-768` oracle | 后续 KAT / 交叉验证 | `OQS_KEM_ml_kem_768_*`；仓库现有 `scripts/liboqs_kem_fixture.py` 仍为 1024 尺寸 | **未跑**（非 E15 本轮门禁） |
| host golden `golden_decrypt` | E15 `m.bin` oracle | `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-decrypt-k3/scripts/host_golden/` 与本 exp `scripts/gen_data.py` | **已验证**：E15 CPU/SIM `m.bin` max=0 |
| 静态 NTT/INTT LUT | Stage2 limb 编码 | 本 exp `scripts/host_golden/f203_ref_common.py` `load_lut_t_i8()` + `scripts/gen_data.py` 写 `lut_*_stacked.bin` | **已验证**：E15 CPU/SIM `m.bin` max=0 |
| ByteDecode / Decompress / Compress1 / ByteEncode1 | 解密首尾编码 | E15/D15 k3 AscendC 自包含副本；`du/dv=10/4`，`c1=960B`、`c2=128B` | **已验证**：E15 CPU/SIM PASS |
| NTT / INTT / inner core | 设备算法 | E15/D15 k3 AscendC 自包含副本（fused 1 launch，AIV 2+1） | **已验证**：CPU PASS；SIM tick **222073** |

---

## 3. 明确禁止

- 用 ML-KEM-1024 fixture 改长度冒充 768。
- 在 golden 路径重写 NTT/CBD/ByteEncode/Compress 核心。
- 缺项时继续写基准计算（须停下补本表）。

---

## 4. 维护

E15 已登记；后续若补 PKE roundtrip / liboqs KAT / NPU，应在本表追加具体命令、fixture 与提交。本表当前仅支撑 768 E15 incubating 预研，不表示 stable-768 可晋级。
