# EN13-encrypt-liboqs-cross · STATUS

> DAG：`D-EXP-EN13`  
> 日期：2026-09-11  
> 结论：**PASS**（CPU + `SIM_DIRECT=1` sim；`c` vs liboqs **max=0**）

---

## 1. 目标

在迁入 cann-ntt 的 Host 多段 Encrypt 链路上，使密文 `c`（1568B）与 liboqs ML-KEM-1024 PKE Encrypt 同种子逐字节一致。

## 2. 相对 EN09 补齐

| 缺口 | 本刀处理 |
|------|----------|
| ek/m/coins | liboqs fixture；ρ=ek[1536:]；Prep 种子=coins |
| t̂ | Host `ByteDecode₁₂` → `input/t_hat.bin` |
| CBD e1/e2 | Host PRF+CBD η=2（nonce 4..8）；**加噪在 Host** |
| μ | Host `Decompress₁(m)` |
| Âᵀ∘ŷ | 设备 SampleNTT 后 **Host 转置** Â→Âᵀ，再喂既有 `enc_matvec_real` |
| ⟨t̂,ŷ⟩ | 新核 `enc_dot_real`（不可 k=1 冒充 matvec） |
| INTT | 修正 `gen_inverse_matrix`：×**3303** 互逆（EN09 ×512 不能对拍 liboqs） |
| Pack | 真 u/v → `output/c.bin` |

## 3. 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | wall | tick | c max |
|------|------|------|------|-------|
| CPU | 0 | **1.831s** | — | **0** |
| SIM | 0 | **132.858s** | **812698** | **0** |

日志：`/opt/cursor/artifacts/enc-encaps-sim/en13-cpu.log`、`en13-sim.log`。  
用例根无 stray `core*.dump`；SIM stray 收拢至 `sim_log/`。

## 4. sync_audit

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  mmad_custom.cpp aiv_func.hpp aic_func.hpp ntt_vec.hpp basic.hpp tiling.h \
  enc_samplentt_real.cpp enc_prep_cbd_real.cpp enc_matvec_real.cpp enc_dot_real.cpp \
  enc_pack_compress_real.cpp enc_aiv_stub_common.hpp \
  --format json
```

| 项 | 值 |
|----|-----|
| 红线 | **0** |
| 非红线 | SYNC-09 性能级（`PIPE_ALL` / EnQue 密度；含 `enc_dot_real`） |
| json | `graph-tests/enc_cann_ntt/EN13-encrypt-liboqs-cross/sync_audit.json` |

## 5. 已锁参数

`NTT_N=256`、`NTT_Q=3329`、`NTT_REF=kyber`、`NTT_BENCH=4`；K=4；η=2；du=11 dv=5；`SEED_D=20260619`；MIX/AIV `blockDim=1`；禁 `-r npu`；禁 GATE 4/8。

## 6. 一条教训

EN09 自洽贯通 ≠ Encrypt：须先让 **Host Alg.14 golden ≡ liboqs**，再接线设备；INTT 矩阵若错（×512），全段 soft 绿也会在交叉上全红。
