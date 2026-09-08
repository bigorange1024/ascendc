# EN01 — 迁入最小 ML-KEM-256 正向 NTT（CPU+SIM）

> DAG：`D-EXP-EN01`  
> 目录（新建）：`graph-tests/enc_cann_ntt/EN01-kem256-ntt-port/`  
> 主控只写本任务书；**subagent 编码**。  
> 运行约束：**仅 CPU + SIM**（NPU 长期占用；禁止 `-r npu`）。

---

## 1. 目标

把 **cann-ntt 族**里能跑 ML-KEM（`n=256,q=3329`）正向 NTT 的**有用代码迁入**本目录，形成 **KernelLaunch 直调**用例：

1. Host 一次 launch（或文档写明的最少 launch）跑完 **ŷ = NTT(y)** 形计算；  
2. **CPU 与 `SIM_DIRECT=1` SIM 均不挂、能正常退出**；  
3. **不对** liboqs / 密文正确性作主门禁（有 golden 对拍更好，失败标 `CORRECTNESS_SOFT_FAIL`，只要不挂可记 PASS-NOHANG）。

**本刀不做**：Encrypt Prep/Matvec/Pack、INTT（留给 EN02）、任何 GATE 4/8、接旧 Tag5T NTT。

---

## 2. 迁入来源（允许）

| 优先级 | 路径 | 怎么用 |
|--------|------|--------|
| **主** | `thirdparty/cann-ntt-author-merged_dsa/` | KernelLaunch 壳 + `mmad_custom` / `aiv_func` / `aic_func` / `ntt_vec` / tiling；**Kyber 档**：`NTT_Q=3329` + `NTT_REF=kyber`（或等价写死） |
| **辅** | `thirdparty/cann-ntt/ntt/op_kernel/`、`…/ntt_data/gen_pqc_data.py` | 只读对照 2-digit BAT / KEM 矩阵；**禁止**搬 ACLNN 注册全栈、`op_host` 图模式 |
| 工程壳 | 本仓 `graph-tests/enc_related/ER03-*` 的 `cmake/`、`run.sh` 模式 | **复制壳**改名；勿复制 ER 核逻辑 |

迁入后代码须落在 **EN01 目录内**（或该目录下 `port/` 子树），**不要**改 `thirdparty/` 源树当运行依赖（可只读拷贝）。

---

## 3. 禁止

- 阅读/抄袭：`examples/**/*encrypt*`、`examples/**/*kem*`、`pass-fix-f203-alg14-*` 核、`frozen/**` 源码、`graph-tests/enc_related/ER0*/` 的 `.cpp/.hpp` 当 NTT 模板  
- 自研 Encrypt GATE 4/8；INTT flag 5/7；Wait 中 SyncAll；自造 SoftSync  
- `-r npu` / 连接真机  
- 否决 cannbot sync_audit 红线  
- 扩大成 Encrypt 全链  
- commit / push / 新分支（无用户授权）

---

## 4. 设计要点（主控 · cannbot）

| Skill | 用途 |
|-------|------|
| `thirdparty/cannbot-skills/ops/ascendc-direct-invoke-template` | 直调工程结构对照（本仓已有 graph-tests 壳优先） |
| `…/ascendc-api-best-practices/references/api-crosscore-sync.md` | 保留迁入核既有 CrossCore；**勿改 flag 语义** |
| `…/ascendc-sync-audit` | 编码后强制 `sync_audit.py`；红线写入 STATUS |

架构对齐本线 KB：**A1/A5/A6** — 本刀 = 独立短 MIX NTT；`q=3329` 2-digit 路径。

---

## 5. 验收（强制）

在 `EN01-kem256-ntt-port/`：

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 项 | 要求 |
|----|------|
| 不挂 | 两模式 **正常结束**（非 timeout 124 / 死等） |
| dump | 用例根无 stray `core*.dump`；SIM 日志按仓规进 OPPROF_/camodel |
| sync_audit | 扫本刀全部设备侧 `.cpp/.hpp`；红线数与 json 路径写入 `STATUS.md`；**禁否决** |
| STATUS.md | 目标、迁入文件列表、命令、结论、红线、一条教训 |
| 中文注释 | 迁入/自改的非平凡逻辑须有中文注释（文件头说明来源与本线角色） |

墙钟：**编码+CPU+SIM ≤ 50min**；超时 ABORT 回报，勿傻等。

---

## 6. 反馈格式（给主控）

```text
EN01: PASS-NOHANG | FAIL | BLOCKED | ABORT
dir: ...
cpu: exit=… 摘要
sim: exit=… 摘要
sync_audit: 红线=N  path=…
ported: 文件列表
lesson: 一句
```

---

## 7. 可读文档（强制先读）

- `docs/notes/Encrypt-cann-ntt-kb.md`  
- `docs/notes/Encrypt-cann-ntt-capability-inventory.md`（N0/G1）  
- `docs/notes/Encrypt-cann-ntt-workmode.md`  
- `docs/rg-encrypt-cann-ntt.yaml`  
- 本文件  

完成后 **不要**改 KB/DAG（主控回收后写）。
