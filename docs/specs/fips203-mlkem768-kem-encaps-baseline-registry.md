# FIPS 203 ML-KEM-768 Alg.20 ML-KEM.Encaps — baseline-registry（草稿）

**主题**：Alg.20 ML-KEM.Encaps（k=3）交付/预研侧 golden / KAT 计算块登记  
**适用（目标）**：`examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-encaps-k3/`（E20 incubating）
**参数卡**：[fips203-mlkem768-parameter-card.md](fips203-mlkem768-parameter-card.md)  
**状态**：E20 预研已验证（CPU + `SIM_DIRECT=1` sim）；liboqs KAT / NPU 未跑

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `ek_kem.bin` | 1184 B | = ek_PKE |
| 输入 | `m.bin` | 32 B | 封装消息 |
| 输出/golden | `c.bin` | 1088 B | 密文 |
| 输出/golden | `K.bin` | 32 B | 共享密钥 |

中间量默认 **禁止**落盘（调试 dump 标非默认）。

---

## 2. 计算块登记

| 计算块 | 用途 | 已验证来源 | 状态 |
|--------|------|------------|------|
| liboqs `ML-KEM-768` oracle | 后续 KAT / 交叉验证 | `OQS_KEM_ml_kem_768_*` / 拟 `liboqs_kem_ref` | **未跑**（非 E20 本轮门禁） |
| host golden `golden_encrypt` | E20 `c.bin` oracle | `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-encaps-k3/scripts/host_golden/` | **已验证**：E20 CPU/SIM `c.bin` max=0 |
| KEM head `H(ek)` / `G(m‖H(ek))` | E20 `K.bin` 与 device `r` oracle | `scripts/gen_data.py` `hashlib.sha3_256/sha3_512`；device 侧 `library/shared/keccak_f1600_kernel` | **已验证**：E20 CPU/SIM `K.bin` max=0 |
| 静态 NTT/INTT LUT | Stage2 limb 编码 | 本 exp `scripts/host_golden/f203_ref_common.py` `load_lut_t_i8()` + `scripts/gen_data.py` 写 `lut_*_stacked.bin` | **已验证**：E20 CPU/SIM `c.bin` max=0 |
| NTT / CBD / Compress / ByteEncode 核心 | 设备算法 | E20 本目录自包含 AscendC；来源为活跃 D20/E14/D14 k3，未使用 `**/frozen/**` | **已验证**：CPU PASS；SIM tick **590261** |

---

## 3. 明确禁止

- 用 ML-KEM-1024 fixture 改长度冒充 768。
- 在 golden 路径重写 NTT/CBD/ByteEncode/Compress 核心。
- 缺项时继续写基准计算（须停下补本表）。

---

## 4. 维护

E20 已登记；后续若补 liboqs KAT / NPU，应在本表追加具体命令、fixture 与提交。
