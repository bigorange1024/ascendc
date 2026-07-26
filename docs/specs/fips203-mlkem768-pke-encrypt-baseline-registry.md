# FIPS 203 ML-KEM-768 Alg.14 K-PKE.Encrypt — baseline-registry（草稿）

**主题**：Alg.14 K-PKE.Encrypt（k=3）交付/预研侧 golden / KAT 计算块登记  
**适用（目标）**：`examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-encrypt-k3/`（E14 incubating）
**参数卡**：[fips203-mlkem768-parameter-card.md](fips203-mlkem768-parameter-card.md)  
**状态**：E14 预研已验证（CPU + `SIM_DIRECT=1` sim）；PKE roundtrip / liboqs KAT / NPU 未跑

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `ek_pke.bin` | 1184 B | 公钥 |
| 输入 | `m.bin` | 32 B | 消息 |
| 输入 | `coins.bin` | 32 B | 加密随机字节；KEM 场景由 `G(m‖H(ek))` 后半派生 |
| 输出/golden | `c.bin` | 1088 B | 密文唯一验收对象 |

中间量默认 **禁止**落盘（调试 dump 标非默认）。

---

## 2. 计算块登记

| 计算块 | 用途 | 已验证来源 | 状态 |
|--------|------|------------|------|
| liboqs `ML-KEM-768` oracle | 后续 KAT / 交叉验证 | `OQS_KEM_ml_kem_768_*`；仓库现有 `scripts/liboqs_kem_fixture.py` 仍为 1024 尺寸 | **未跑**（非 E14 本轮门禁） |
| host golden `golden_encrypt` | E14 `c.bin` oracle | `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-pke-encrypt-k3/scripts/host_golden/` | **已验证**：E14 CPU/SIM `c.bin` max=0 |
| 静态 NTT/INTT LUT | Stage2 limb 编码 | 本 exp `scripts/host_golden/f203_ref_common.py` `load_lut_t_i8()` + `scripts/gen_data.py` 写 `lut_*_stacked.bin` | **已验证**：E14 CPU/SIM `c.bin` max=0 |
| CBD η=2 / PRF coins | 生成 `r‖e1‖e2 [7,256]` | E14 自包含 `prep/` 与 `scripts/host_golden/golden_c.py`；输入 `coins.bin` 为 32B | **已验证**：E14 CPU/SIM PASS |
| NTT / INTT / Compress / ByteEncode 核心 | 设备算法 | E14/D14 k3 AscendC 自包含副本（Â[9]、`re[7]`、INTT batch4、`du/dv=10/4`） | **已验证**：CPU PASS；SIM tick **507633** |

---

## 3. 明确禁止

- 用 ML-KEM-1024 fixture 改长度冒充 768。
- 在 golden 路径重写 NTT/CBD/ByteEncode/Compress 核心。
- 缺项时继续写基准计算（须停下补本表）。

---

## 4. 维护

E14 已登记；后续若补 PKE roundtrip / liboqs KAT / NPU，应在本表追加具体命令、fixture 与提交。本表当前仅支撑 768 E14 incubating 预研，不表示 stable-768 可晋级。
