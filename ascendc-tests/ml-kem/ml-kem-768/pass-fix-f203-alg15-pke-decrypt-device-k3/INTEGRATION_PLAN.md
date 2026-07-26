# INTEGRATION_PLAN — pass-fix-f203-alg15-pke-decrypt-device-k3

**定位**：FIPS 203 **Algorithm 15 K-PKE.Decrypt**（ML-KEM-768 / k=3）的 device 探针。

**实现来源**：从活跃 `ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4/` 复制后 retarget；复用 k3 D13/D14 的尺寸、golden 与 AIV 2+1 分片经验。禁止从 `frozen/` 带出源码或路线。

**状态**：**PASS**（2026-07-26）；单 fused launch，host `blockDim=1`。

---

## 1. 锁定 I/O 与标准流程

```text
dk_PKE ∈ B^{384k} = 1152B
c      ∈ B^{32(d_u k + d_v)} = 1088B
       c1 = 3×320B, d_u=10
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
| **参数** | `k=3`、`du=10`、`dv=4`；不得回落到 k4 的 `dk=1536`、`c=1568`、`du=11`、`dv=5` |
| **Launch** | 生产 / SIM 为 **1** 个 `f203_decrypt_device_fused` |
| **Host 输出** | 仅 D2H `m.bin`；中间态只作为设备 GM workspace |
| **Golden** | Host golden 只作 I/O oracle；设备实现不追求源码同构 |

---

## 2. 设备编排

| 段 | 说明 |
|----|------|
| prep | AIV0 标量 ByteDecode₁₀/₄/₁₂；Decompress₁₀/₄ 走统一整数向量路径 |
| NTT(u') | Stage123 k=3；AIV0 处理 poly0/1，AIV1 处理 poly2 |
| su_dot | AIV0 计算 `ŵ = <ŝ, û>`；按 k=3 pad 到 INTT 输入 polyvec 前缀 |
| INTT | 复用 Stage123；尾段只消费第 0 个 poly 的时域输出 |
| tail | `v' - w_time` 向量约化；Compress₁ Barrett 向量；ByteEncode₁ 标量 LSB-first pack |

同步沿用 k4 PASS 的单核 FSM：NTT flag 1/2/3，INTT flag 1/3，段间 GATE 4→8；prep 与 su_dot 通过 `softSyncGm` 让 AIV0/AIV1 汇合。

---

## 3. 验收记录

```bash
cd ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg15-pke-decrypt-device-k3
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | 结果 |
|------|------|
| CPU | **PASS**；`[verify] PASS max=0 (32 bytes)`；`m.bin=32B` |
| SIM | **PASS**；`[verify] PASS max=0 (32 bytes)`；Total tick **222032**；Model RUN TIME **34228.8 ms** |

根目录 stray：`core*.dump` / `profile_*_log*.toml` 为 **none**。

---

## 4. 后续

- W2 PKE 三段（D13/D14/D15）已具备 CPU + SIM 绿证据。
- D15 尚未接独立 D14→D15 roundtrip 脚本；当前完成独立 `dk_pke+c→m` 最小验收。
- 下一阶段是 W3 KEM device（D19/D20/D21/CT），仍按 ML-KEM-768 参数卡另行授权。
