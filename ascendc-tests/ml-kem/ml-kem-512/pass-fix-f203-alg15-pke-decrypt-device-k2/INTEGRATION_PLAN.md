# INTEGRATION_PLAN — pass-fix-f203-alg15-pke-decrypt-device-k2

**定位**：FIPS 203 **Algorithm 15 K-PKE.Decrypt**（ML-KEM-512 / k=2）的 device 探针。

**实现来源**：从活跃 `ml-kem-768/pass-fix-f203-alg15-pke-decrypt-device-k3/` 复制后 retarget；按 ML-KEM-512 锁定尺寸与 AIV 1+1 分片改造。禁止从 `frozen/` 带出源码或路线。

**状态**：**D15 PASS**；单 fused launch，host `blockDim=1`。

---

## 1. 锁定 I/O 与标准流程

```text
dk_PKE ∈ B^{384k} = 768B
c      ∈ B^{32(d_u k + d_v)} = 768B
       c1 = 2×320B, d_u=10
       c2 = 128B,   d_v=4
m      ∈ B^{32}

Alg.15:
1–2  split c -> c1, c2
3    u' <- Decompress_10(ByteDecode_10(c1))
4    v' <- Decompress_4(ByteDecode_4(c2))
5    s  <- ByteDecode_12(dk_PKE)
6    w  <- v' - NTT^-1(s^T o NTT(u'))
7    m  <- ByteEncode_1(Compress_1(w))
```

| 不变量 | 说明 |
|--------|------|
| **参数** | `k=2`、`du=10`、`dv=4`；不得回落到 k3/k4 的旧尺寸或旧分片 |
| **Launch** | 生产 / SIM 为 **1** 个 `f203_decrypt_device_fused` |
| **Host 输出** | 仅 D2H `m.bin`；中间态只作为设备 GM workspace |
| **Golden** | Host golden 只作 I/O oracle；设备实现不追求源码同构 |

---

## 2. 设备编排

| 段 | 说明 |
|----|------|
| prep | AIV0 标量 ByteDecode₁₀/₄/₁₂；Decompress₁₀/₄ 走统一整数向量路径 |
| NTT(u') | Stage123 k=2；AIV0 处理 poly0，AIV1 处理 poly1 |
| su_dot | AIV0 计算 `ŵ = <ŝ, û>`；按 k=2 pad 到 INTT 输入 polyvec 前缀 |
| INTT | 复用 Stage123；尾段只消费第 0 个 poly 的时域输出 |
| tail | `v' - w_time` 向量约化；Compress₁ Barrett 向量；ByteEncode₁ 标量 LSB-first pack |

同步沿用活跃 D15 单核 FSM：NTT flag 1/2/3，INTT flag 1/3，段间 GATE 4→8；prep 与 su_dot 通过 `softSyncGm` 让 AIV0/AIV1 汇合。

---

## 3. 验收记录

```bash
cd ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg15-pke-decrypt-device-k2
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | 结果 |
|------|------|
| CPU | **PASS**；`[verify] PASS max=0 (32 bytes)`；`m.bin=32B` |
| SIM | **PASS**；`[verify] PASS max=0 (32 bytes)`；Total tick **168975**；Model RUN TIME **20906.4 ms** |

根目录 stray：`core*.dump` / `profile_*_log*.toml` 为 **none**。

---

## 4. 后续

- 当前分支未见 D14 k2 目录；D15 先按独立 `dk_pke+c→m` 自包含 golden 验收。
- 下一阶段是 W3 KEM device（D19/D20/D21/CT），仍按 ML-KEM-512 参数卡另行授权。
