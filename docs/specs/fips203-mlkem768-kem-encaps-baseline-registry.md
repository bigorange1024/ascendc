# FIPS 203 ML-KEM-768 Alg.20 ML-KEM.Encaps — baseline-registry（草稿）

**主题**：Alg.20 ML-KEM.Encaps（k=3）交付/预研侧 golden / KAT 计算块登记  
**适用（目标）**：`examples/incubating/ml-kem/ml-kem-768/` 对应 `exp-*`（**尚未实现**）  
**参数卡**：[fips203-mlkem768-parameter-card.md](fips203-mlkem768-parameter-card.md)  
**状态**：骨架 · 计算块多为 **未验证** · **禁止**在未补登记前当交付真源

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

## 2. 计算块登记（待补绿）

| 计算块 | 用途 | 已验证来源 | 状态 |
|--------|------|------------|------|
| liboqs `ML-KEM-768` oracle | golden I/O | `OQS_KEM_ml_kem_768_*` / 拟 `liboqs_kem_ref` | **未验证胶水**（P2 补） |
| 静态 NTT LUT | limb 编码 | 待定（须与设备同 LUT） | **未验证** |
| Derand `d`/`z` | host/device 同式 | 参数卡 §4 域分离串 | **草案锁定 / 实现未验** |
| NTT / CBD / Compress / ByteEncode 核心 | 设备算法 | AscendC（768 树） | **未实现** — 禁止在 golden 路径重写核心 |

---

## 3. 明确禁止

- 用 ML-KEM-1024 fixture 改长度冒充 768。
- 在 golden 路径重写 NTT/CBD/ByteEncode/Compress 核心。
- 缺项时继续写基准计算（须停下补本表）。

---

## 4. 维护

P2 每通过一块 → 把「未验证」改为具体路径与提交；进 INDEX 时同步日期。
