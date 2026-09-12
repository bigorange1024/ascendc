# 2026-09-12 · 单 AIV ML-KEM NTT 抽取与 SIM 对比

## 结论

- `thirdparty/ntt`：**支持**单 AIV、单 poly、ML-KEM（q=3329）。
- 已抽取探针 `graph-tests/aiv_ntt/AV01-single-aiv-mlkem-ntt/`；CPU+SIM golden PASS；SIM tick **7871**。
- 对比 EN01 cann-ntt：批 4-poly Cube 仍更优（11029）；单 poly 对位 AV01 不差。
- **采纳为 AIV NTT 基础积木**；基于它重写 Encrypt/Encaps **需另立项**（先扩 polyvec 或接受 launch×k）。

## 假绿三问（对拍）

1. golden 与实现同源？否——host 用独立 DFT Oracle（`reference.hpp` / `gen_data.py`）。  
2. 权威交叉？本刀为 NTT 单算子 I/O；Encrypt 级 liboqs 交叉不在范围。  
3. 跨核 Sync？AIV-only，无 CrossCore。
