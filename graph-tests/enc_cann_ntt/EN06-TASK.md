# EN06 — Pack 换真 Compress + ByteEncode（CPU+SIM）

> DAG：`D-EXP-EN06`  
> 目录（新建）：`graph-tests/enc_cann_ntt/EN06-pack-compress-realbrick/`  
> 基线：复制 `EN05-prep-sample-realbrick/`（勿改 EN01–05）  
> **仅 CPU+SIM**；主目标不挂。

---

## 1. 目标

五段 Host 不变；将 **L5 Pack** 从桩升级为真尾部（能力 C2/C3，对齐 ML-KEM-1024）：

- **Compress** \(d_u=11,\ d_v=5\)（或本刀声明的等价子集）  
- **ByteEncode** 打包为密文外形 \(c\)（可简化 batch，须 STATUS 写清）  
- 独立 AIV launch（X32）；禁 GATE 4/8、禁与 NTT 融核  
- 可用 `library/shared/f203_unified_round/`、`f203_byte_codec/`；按笔记**重写**，禁抄 Encrypt pack / alg14 tail 核整文件  

Prep/Matvec/NTT/INTT 沿用 EN05。主门禁不挂；Pack 对拍尽量做，可 soft-fail。

---

## 2. 允许阅读

- `F203-Compress-Decompress-向量实现指南.md`、`F203-ByteEncode-ByteDecode-d-向量与标量选型.md`  
- 单功能探针 STATUS：`pass-f203-compress-d-vec-k4`、`pass-f203-byteencode-d-vec-k4`、`pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4`（**仅契约**）  
- EN05 树复制  

## 3. 禁止

抄 Encrypt/alg14/ER 核；NPU；commit/push；改 KB/DAG；否决 sync 红线。

## 4. 验收

```bash
cd graph-tests/enc_cann_ntt/EN06-pack-compress-realbrick
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

sync_audit；STATUS；墙钟 ≤70min。

## 5. 反馈

```text
EN06: PASS-NOHANG | FAIL | BLOCKED | ABORT
dir: ...
pack: 覆盖范围一句
cpu/sim: ...
sync_audit: 红线=N path=…
lesson: 一句
```
