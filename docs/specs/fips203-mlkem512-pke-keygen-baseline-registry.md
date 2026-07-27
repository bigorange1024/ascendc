# FIPS 203 ML-KEM-512 Alg.13 K-PKE.KeyGen — baseline-registry（骨架）

**主题**：Alg.13 K-PKE.KeyGen（k=2）交付/预研侧 golden / KAT 计算块登记  
**适用（目标）**：`examples/incubating/ml-kem/ml-kem-512/exp-fips203-mlkem-pke-keygen-k2/`  
**参数卡**：[fips203-mlkem512-parameter-card.md](fips203-mlkem512-parameter-card.md)  
**状态**：**P0 骨架**（2026-07-27）；incubating 绿后补登记

---

## 1. 生产 I/O（黑盒）

| 角色 | 尺寸 | 说明 |
|------|------|------|
| 输入 `seed_d` | 4 B | uint32 LE |
| 输出 `ek_pke` | **800 B** | ByteEncode12(t̂)‖ρ |
| 输出 `dk_pke` | **768 B** | ByteEncode12(ŝ) |

中间量默认 **禁止**落盘（调试 dump 标非默认）。

---

## 2. 计算块登记

| 计算块 | 用途 | 已验证来源 | 状态 |
|--------|------|------------|------|
| liboqs `ML-KEM-512` oracle | KAT / 交叉 | `OQS_KEM_ml_kem_512_*`；`MLKEM_PARAM=512` fixture | **待登记**（glue 必达） |
| CBD η=3 | KeyGen 噪声 | `library/shared/fips203_se_sample` `sample_poly_cbd3` + liboqs `mlk_poly_cbd3` 对拍 | **补缺中** |
| NTT / ByteEncode12 | 设备算法 | 待 512 探针/exp 获证后登记 | 待补 |

---

## 3. 明确禁止

- 用 ML-KEM-768/1024 fixture 改长度冒充 512。
- 在 golden 路径重写未登记的 NTT/CBD/ByteEncode/Compress 核心。
- 缺项时继续写基准计算（须停下补本表）。

---

## 4. 维护

P0 仅骨架；各波次补缺后在本表追加命令、fixture 与提交。
