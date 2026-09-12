# EN13 — Encrypt × liboqs PKE 交叉（SIM-only）

> DAG：`D-EXP-EN13`  
> 目录（新建）：`graph-tests/enc_cann_ntt/EN13-encrypt-liboqs-cross/`  
> 主控写任务书；**编码可本会话或 subagent**。  
> **运行**：仅 `cpu` + `SIM_DIRECT=1 sim`；**禁止 `-r npu`**。

---

## 1. 目标

在 **迁入 cann-ntt** 的 Host 多段 Encrypt 形链路上，使密文 **`c`（1568B）与 liboqs ML-KEM-1024 PKE Encrypt 同种子逐字节一致**（回答 `Q-ENCRYPT-LIBOQS-BYTE`）。

不挂为底线；本刀 **正确性升级为主门禁**。

---

## 2. 起点与禁令

| 允许 | 禁止 |
|------|------|
| 复制壳：`EN09-samplentt-device/`（改名 EN13） | 抄 `examples/**/*encrypt*` 核、`frozen/**`、`enc_related/ER0*` / `RB-T*` **核源码** |
| 只读：能力清单、KB、EN09 STATUS、shared `f203_*`、`scripts/liboqs_pke_*` | 把 Tag5T 核当 NTT；胖 MIX 融 NTT+Matvec |
| 复用迁入 cann-ntt NTT/INTT | 否决 sync_audit 红线 |

---

## 3. 相对 EN09 必须补的语义

EN09 自洽贯通 **≠** Alg.14。至少补：

1. 输入：`ek_pke`(1568)=`t‖ρ`、`m`(32)、`coins`/`r`(32)（liboqs fixture）  
2. `t̂ ← ByteDecode₁₂(t)`  
3. CBD：`y‖e1‖e2`（nonce 按 FIPS；η=2）  
4. `ŷ=NTT(y)`（cann-ntt）  
5. `û = Âᵀ∘ŷ`（Â 自 SampleNTT(ρ)）；`u = INTT(û)+e1`  
6. `v = INTT(t̂ᵀ∘ŷ)+e2+Decompress₁(m)`  
7. `c = ByteEncode(Compress₁₁(u)‖Compress₅(v))`  
8. verify：`c` ≡ `liboqs_pke_ref_mlkem1024 encrypt`（max=0）

架构：NTT/INTT **独立短 MIX launch**；其余尽量 AIV/Host；禁 GATE 4/8。

---

## 4. 验收

```bash
cd graph-tests/enc_cann_ntt/EN13-encrypt-liboqs-cross
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# sync_audit 红线=0（有同步则必跑）
```

| 门禁 | 标准 |
|------|------|
| 不挂 | cpu+sim 正常退出；无 stray dump 在用例根 |
| 正确性 | `c` vs liboqs **max=0** |
| 同步 | 红线 0 |
| 禁 | `-r npu` |

SIM 预算：建议 ≤1800s（对齐 EN09 量级）。

---

## 5. 反馈

写 `graph-tests/enc-encaps-cann-ntt-sim/tasks/EN13/FEEDBACK.md`：PASS/FAIL/BLOCKED + 命令 + tick + 红线 + 一句教训。

---

## 6. 可读绝对路径

- `/workspace/graph-tests/enc_cann_ntt/EN09-samplentt-device/`
- `/workspace/docs/notes/Encrypt-cann-ntt-capability-inventory.md`
- `/workspace/docs/notes/Encrypt-cann-ntt-kb.md`
- `/workspace/scripts/liboqs_pke_ref_mlkem1024.c` / `liboqs_pke_fixture.py` / `build_liboqs_pke_ref_mlkem1024.sh`
- `/workspace/graph-tests/enc-encaps-cann-ntt-sim/PLAN.md`
