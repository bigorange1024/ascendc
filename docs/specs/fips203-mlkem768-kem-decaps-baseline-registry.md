# FIPS 203 ML-KEM-768 Alg.21 ML-KEM.Decaps — baseline-registry（草稿）

**主题**：Alg.21 ML-KEM.Decaps（k=3）交付/预研侧 golden / KAT 计算块登记  
**适用（目标）**：`examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-decaps-k3/`（E21）与 `exp-fips203-mlkem-kem-decaps-ct-k3/`（E21ct）
**参数卡**：[fips203-mlkem768-parameter-card.md](fips203-mlkem768-parameter-card.md)  
**状态**：E21 / E21ct 预研均已验证（accept/reject CPU + `SIM_DIRECT=1` sim）；liboqs KAT / NPU 未跑

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `dk_kem.bin` | 2400 B | 解封密钥 |
| 输入 | `c.bin` | 1088 B | 密文 |
| 输出/golden | `K.bin` | 32 B | 合法或拒绝路径共享密钥 |

中间量默认 **禁止**落盘（调试 dump 标非默认）。

---

## 2. 计算块登记

| 计算块 | 用途 | 已验证来源 | 状态 |
|--------|------|------------|------|
| liboqs `ML-KEM-768` oracle | 后续 KAT / 交叉验证 | `OQS_KEM_ml_kem_768_*`；仓库现有 `scripts/liboqs_kem_fixture.py` 仍为 1024 尺寸 | **未跑**（非 E21/E21ct 本轮门禁） |
| E19 KEM KeyGen oracle | 生成 `dk_kem=dk_pke‖ek‖H(ek)‖z` | E21/E21ct `scripts/keygen_golden.py`（vendored 自 E19/E13 已绿 oracle） | **已验证**：E21 / E21ct accept+reject CPU/SIM 均 PASS |
| E15 PKE Decrypt oracle | Phase-D `m'` 与合法路径对拍辅助 | E21/E21ct vendored E15/D15 k3 Decrypt fused 1 launch | **已验证**：E21 tick D**221059**；E21ct tick D**220727**（accept SIM） |
| E14 PKE Encrypt oracle | Phase-E 重加密与 FO 比较 | E21/E21ct vendored E14/D14 k3 Encrypt 几何；`scripts/host_golden/` 生成合法 `c`/`K` | **已验证**：E21 tick E**599171**；E21ct tick E**605388**（accept SIM） |
| 静态 NTT/INTT LUT | Decrypt/Encrypt limb 编码 | E21/E21ct `scripts/host_golden/f203_ref_common.py` `load_lut_t_i8()` + `scripts/gen_data.py` 写 `lut_*_stacked.bin` | **已验证**：accept/reject CPU/SIM `K.bin` max=0 |
| `H(ek)` / `G(m‖H(ek))` / `J(z‖c)` | FO accept/reject shared secret | Host golden `hashlib.sha3_256/sha3_512/shake_256`；device `library/shared/keccak_f1600_kernel` | **已验证**：E21 accept **820230**, reject **822500**；E21ct accept **826115**, reject **825836** |
| CT 调度 | E21ct 常时专题路径 | `exp-fips203-mlkem-kem-decaps-ct-k3/` 默认 `ASCENDC_SIM_HOST_MODE=decaps_2session` | **已验证**：E21ct accept/reject CPU/SIM PASS；非默认脚本路径 |
| AscendC-only KEM roundtrip glue | E21 默认无 `-ct` 端到端 accept/reject | `scripts/exp_kem768_liboqs_roundtrip.sh`（当前不使用 liboqs-768；见脚本头注释） | **已验证**：CPU×1 + SIM×1 PASS；accept `K` max=0；reject `K=J(z‖c_bad)` max=0 且 `K_reject != K_accept` |

---

## 3. 明确禁止

- 用 ML-KEM-1024 fixture 改长度冒充 768。
- 在 golden 路径重写 NTT/CBD/ByteEncode/Compress 核心。
- 缺项时继续写基准计算（须停下补本表）。

---

## 4. 维护

E21 / E21ct 已登记；后续若补 liboqs KAT / NPU，应在本表追加具体命令、fixture 与提交。本表当前仅支撑 768 E21/E21ct incubating 预研与 AscendC-only roundtrip，不表示 stable-768 可晋级。
