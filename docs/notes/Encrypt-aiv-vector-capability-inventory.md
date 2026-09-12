# 单 AIV 全向量 · 能力清单（ML-KEM-1024 / Alg.14+20）

> 刷新：2026-09-12 · 与 [`Encrypt-aiv-vector-kb.md`](Encrypt-aiv-vector-kb.md) / [`ASSETS.md`](../../graph-tests/aiv-kem-vector-sim/ASSETS.md) 对齐

| Alg | 步骤 | 能力 | 向量门槛 | 状态 |
|-----|------|------|----------|------|
| 14 | 输入 ek,m,r | ByteDecode₁₂(t̂)、ρ | 设备标量积木 + 可 Vec | **AE-E DEVICE_FULL** |
| 14 | 扩 Â、采样 y/e1/e2 | SHAKE128 SampleNTT×16 + SHAKE256 CBD | 设备 Keccak（Shake128 已补） | **AE-E DEVICE_FULL** |
| 14 | NTT(y) | 正向 NTT×4 | Vec Gather/蝶形 | **AV01→AE-E** |
| 14 | Âᵀ∘ŷ, ⟨t̂,ŷ⟩ | NTT 域 matvec/dot | Vec Mul/Add | **AE-E** |
| 14 | INTT(û), INTT(v̂) | 逆向 NTT | Vec 同族 | **AE-E 新建** |
| 14 | +e、+μ | mod add、Decompress₁ | Vec/标量混合 | **AE-E** |
| 14 | Compress+ByteEncode | du=11,dv=5 | 打包路径 | **AE-E** |
| 20 | H(ek), G(m‖H) | SHA3-256/512 | 设备 Keccak | **AE-P DEVICE_FO** |
| 20 | Encrypt + K | 调 Encrypt 变换 | — | **AE-P DEVICE_FULL** |

## 扩展能力（架构，未开战役）

| 能力 | 说明 | 状态 |
|------|------|------|
| 单 AIV 串多例 | 同核按序多 `(ek,m)` | 设计已锁；未实现批调度 |
| 多 AIV 并行多例 | 1 AIV = 1 任务，无跨任务 Sync | 设计已锁；未实现 |

图例：DEVICE_FULL = Host 不预喂密码学中间态。
