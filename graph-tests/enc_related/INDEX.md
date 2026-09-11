# graph-tests/enc_related

> Wave C Encrypt 拼装实现目录（RB-T07…）。

| 目录 | 刀 | 说明 |
|------|----|------|
| [`RB-T07-prep-shell/`](RB-T07-prep-shell/) | T07 / G4 | Host：ρ←ek 尾；coins→(y,e₁,e₂)；无设备核 |
| [`RB-T08-uv-topology/`](RB-T08-uv-topology/) | T08 / G3 | Host：预喂 Â/ŷ/t̂/e₁/e₂/μ → u,v 拓扑；无设备核 |
| [`RB-T09-encrypt-shaped-2launch/`](RB-T09-encrypt-shaped-2launch/) | T09 | 外形双 launch：prep(AIV)+compute(MIX 1/3+GATE4+pack)；SIM 不挂 |
| [`RB-T10-uv-device-mix/`](RB-T10-uv-device-mix/) | T10 | 单 launch MIX：设备 MultiplyNTTs+INTT → u,v |
| [`RB-T11-encrypt-shaped-device-uv/`](RB-T11-encrypt-shaped-device-uv/) | T11 | 外形双 launch + 设备 u,v + pack→c |
| [`RB-T12-device-ntt-y/`](RB-T12-device-ntt-y/) | T12 | 单 launch MIX：设备 ŷ←NTT(y)（接 prep CBD） |
| [`RB-T13-device-sample-ntt-a/`](RB-T13-device-sample-ntt-a/) | T13 | 单 launch MIX：设备 Â←SampleNTT(ρ)（接 T07 ρ 尾） |
| [`RB-T14-device-ahat-yhat/`](RB-T14-device-ahat-yhat/) | T14 | 单 launch MIX：设备 Â←SampleNTT(ρ) ‖ ŷ←NTT(y)（双对拍） |
| [`RB-T15-encrypt-ahat-yhat-uv-c/`](RB-T15-encrypt-ahat-yhat-uv-c/) | T15 | 双 launch：设备 Â+ŷ → Mul/INTT→u,v → pack→c |
| [`RB-T16-device-cbd-y-e/`](RB-T16-device-cbd-y-e/) | T16 | 单 launch MIX：设备 coins→PRF→CBD→(y,e₁,e₂) |
| [`RB-T17-encrypt-full-device/`](RB-T17-encrypt-full-device/) | T17 | 双 launch：设备 CBD+Â/ŷ→u,v→c（无 Host 预喂噪声） |
| [`RB-T18-device-bytedecode12-ek/`](RB-T18-device-bytedecode12-ek/) | T18 | 单 launch MIX：设备 ByteDecode₁₂(ek)→t̂ |
| [`RB-T19-encrypt-ek-decode-full/`](RB-T19-encrypt-ek-decode-full/) | T19 | 双 launch：设备 Decode₁₂(ek)+CBD+Â/ŷ→u,v→c（无 Host 预喂 t̂） |
| [`RB-T20-encaps-shaped/`](RB-T20-encaps-shaped/) | T20 | Encaps 外形：Host m→μ + K；设备 Encrypt 全链→c |
| [`RB-T21-encaps-device-mu/`](RB-T21-encaps-device-mu/) | T21 | T20 + 设备 μ←Decompress₁(m)；Host 不预喂最终 μ |
| [`RB-T22-encaps-device-G/`](RB-T22-encaps-device-G/) | T22 | T21 + 设备 (K‖r)←G(m‖H(ek))；c 与 K 对拍 |
| [`RB-T23-encaps-liboqs-cross/`](RB-T23-encaps-liboqs-cross/) | T23 | T22 设备 Encaps × liboqs ML-KEM-1024 交叉对拍 c/K |
| [`RB-T24-encaps-decaps-roundtrip/`](RB-T24-encaps-decaps-roundtrip/) | T24 | 设备 Encaps(c,K) → liboqs Decaps(sk,c)→K' ≡ K |
| [`RB-T25-decrypt-device/`](RB-T25-decrypt-device/) | T25 | Alg.15 Decrypt：三 launch prep→NTT→INTT+extract → m；liboqs PKE Decrypt golden |
| [`RB-T26-decaps-device/`](RB-T26-decaps-device/) | T26 | Alg.21 Decaps：Decrypt(T25)+Reenc(T22/23) 五 launch → K；liboqs Decaps golden |
| [`RB-T27-encaps-decaps-device-rt/`](RB-T27-encaps-decaps-device-rt/) | T27 | 设备 Encaps→设备 Decaps 往返；K'≡K；liboqs 交叉；X12 DataCopy 写出 |

| [`RB-T28-decaps-4launch/`](RB-T28-decaps-4launch/) | T28 / LR-DP-F1 | Decaps 4 launch：Decrypt(NTT+INTT 融) + Encaps2；**PASS_NPU×30** |
| [`RB-T29-decaps-3launch/`](RB-T29-decaps-3launch/) | T29 / LR-DP-F2 | Decaps 3 launch：Decrypt(prep+NTT+INTT 融) + Encaps2；**PASS_NPU×30** |
| [`RB-T30-decaps-2launch/`](RB-T30-decaps-2launch/) | T30 / LR-DP-F3 | Decaps **2** launch：**PASS_NPU×30**；Σ **1719µs** · [NPU表](../../qa/active_npu_perf_summary.md) |

运营任务书：[`../encrypt-rebuild-ops/`](../encrypt-rebuild-ops/INDEX.md)。

Encrypt **相关拼装**实验区（toys 结构维已穷尽 → 本区 dig 近生产体量）。

| 刀 | 任务书 / 目录 | 状态 |
|----|----------------|------|
| ER01 | [ER01-TASK.md](ER01-TASK.md) → [`ER01-encrypt-shaped-2launch-skel/`](ER01-encrypt-shaped-2launch-skel/) | **PASS**（CPU+SIM；sync_audit 红线 16→交 ER02） |
| ER02 | 清 ER01 的 cannbot **SYNC-02** 红线（禁否决） | **PASS**（就地修 ER01；红线 0） |
| ER03 | [ER03-TASK.md](ER03-TASK.md) → [`ER03-gate-realbrick-volume/`](ER03-gate-realbrick-volume/) | **PASS**（MAC 256×32；红线 0；X27） |
| ER04 | [ER04-TASK.md](ER04-TASK.md) → [`ER04-cube-ntt-volume/`](ER04-cube-ntt-volume/) | **PASS**（Cube×16；tick≈85889；X28） |
| ER05 | [ER05-TASK.md](ER05-TASK.md) → [`ER05-sticky-dual-compute/`](ER05-sticky-dual-compute/) | **PASS**（粘性×2；X29） |

**节奏**：旧 hang 线 `D-SIM-FRONTIER-PAUSE` 仍有效。  
**当前主线**已迁至 [`../enc_cann_ntt/`](../enc_cann_ntt/INDEX.md)（Host + 迁入 cann-ntt）。本目录 ER01–05 **只读教训**，不再叠同质 SIM 加压刀。

**方法（旧线）**：图谱 + hang-KB；cannbot sync-audit。  
**禁止**：抄旧 Encrypt；未跑 sync_audit 声称完成。

