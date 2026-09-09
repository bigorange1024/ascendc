# NPU / 实验状态（人话）

> 刷新：2026-09-09 17:34

## 战役目标（是否达成）

| 目标 | 结果 |
|------|------|
| Decrypt 明文 `m` 与 **liboqs** 一致，且 NPU 不挂 | **达成**（全链 + 加压） |
| Decaps 共享密钥 `K` 与 **liboqs** 一致（合法/拒绝） | **达成** |
| 设备 Encaps↔Decaps 往返 `K` 一致 + 反复跑不挂 | **达成**（×30 + 约 100 次收口） |

云机：**已停心跳放机**（当前无在跑作业）。本刷新为文档/图谱分析，无新上板。

## 各实验在测什么、结果是什么

| 代号 | 测什么（算法一步） | 对拍标准 | 结果 |
|------|-------------------|----------|------|
| D01 | 私钥/密文 **unpack** → ŝ,u,v | 与 host/FIPS oracle 一致 | NPU 绿；再压 **30/30** |
| D02 | **NTT(u)+内积** → û,ŵ | 同上 | NPU 绿；**30/30** |
| D03 | **INTT+抽出** → 明文 m(32B) | 同上 | NPU 绿；**30/30** |
| D04 | 上面三步串成 **完整 Decrypt** | `m` ≡ liboqs PKE Decrypt | NPU 绿；**30/30** |
| K01 | Decaps 里设备 **G(m‖h)** → (K',r') | 与 sha3/host 一致 | NPU 绿；**30/30** |
| K02 | **再加密** 得 c' | `c'` ≡ liboqs Encrypt | NPU 绿；**30/30** |
| K03 | **FO**：合法用 K' / 拒绝用 J(z‖c) | 两路 `K` ≡ liboqs Decaps | NPU 绿；**30/30** |
| K04 | 设备 Encaps 再设备 Decaps **往返** | `K_dec≡K_enc≡liboqs` | NPU×30 + 累计约×100 + 抽测×10 全绿 |

细表：[`MATRIX.md`](MATRIX.md)。原理：[`MIX-Decrypt-Decaps-反卡死拓扑技术总结.md`](../../docs/notes/MIX-Decrypt-Decaps-反卡死拓扑技术总结.md) §4（为何不挂 + KeyGen 对照）。KB：[`Decrypt-cannbot-rebuild-kb.md`](../../docs/notes/Decrypt-cannbot-rebuild-kb.md) §B2.1–B2.2。
