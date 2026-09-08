# EN02 — NTT→INTT 双 launch 粘性（CPU+SIM）

> DAG：`D-EXP-EN02`  
> 基线：**复制** `EN01-kem256-ntt-port/` → 新目录（勿改 EN01）  
> 目录（新建）：`graph-tests/enc_cann_ntt/EN02-ntt-intt-2launch/`  
> **仅 CPU + SIM**；禁 NPU。主控不写核。

---

## 1. 目标

在迁入的 cann-ntt 积木上验证 **Host 连续两段 launch 不挂**：

1. Launch1：**正向** NTT（同 EN01：`q=3329`，BAT/`M` 正向）  
2. Launch2：**逆向** INTT（换 inverse 矩阵；同一 kernel 类 / 同构 MIX，仅 Host 换 `M4`+输入）  
3. 同进程、同 device session 内 **连续跑完**（粘性）；主门禁 **不挂**  
4. 正确性次要：若易做「NTT∘INTT≈id（差缩放）」可记；失败不挡 PASS-NOHANG

**不做**：Encrypt Prep/Matvec/Pack、GATE 4/8、接旧 Tag5T、改 EN01 目录。

---

## 2. 允许来源

| 来源 | 用法 |
|------|------|
| `graph-tests/enc_cann_ntt/EN01-kem256-ntt-port/` | **整目录复制**为起点（含已修 SYNC-02） |
| `thirdparty/cann-ntt/.../gen_pqc_data.py` 中 `mlkem_inverse_ntt` / inverse BAT | **只读**；把 inverse 矩阵生成逻辑迁入本刀 `scripts/`（可改编，中文注释） |
| 作者包 | 仅当 EN01 未覆盖的辅助；优先 EN01 树 |

---

## 3. 禁止

同 EN01-TASK §3；另：**禁止**把 NTT+INTT 融回单核自研 GATE；禁止 `-r npu`；禁止否决 sync 红线；禁止 commit/push。

---

## 4. cannbot

- 读 `api-crosscore-sync.md`（若改握手）  
- 编码后：`sync_audit.py` 扫本刀设备侧源 → STATUS；红线禁否决  
- 继承 KB：**X30**（迁入/改探针先 audit）、**B1–B7**

---

## 5. 验收

在 `EN02-ntt-intt-2launch/`：

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 项 | 要求 |
|----|------|
| 不挂 | 两模式正常结束；Host 内 **NTT launch + INTT launch** 均完成 |
| dump | 根无 stray core dump |
| sync_audit | 红线写入 STATUS |
| STATUS.md | 两段 launch 说明、命令、tick/墙钟、红线、一句教训 |
| 墙钟 | ≤50min；超时 ABORT |

---

## 6. 反馈格式

```text
EN02: PASS-NOHANG | FAIL | BLOCKED | ABORT
dir: ...
cpu: ...
sim: ...
sync_audit: 红线=N path=…
launches: NTT+INTT 说明
lesson: 一句
```

## 7. 必读

`Encrypt-cann-ntt-kb.md`（含 S1/X30）、`EN01-TASK.md`、本文件、`rg-encrypt-cann-ntt.yaml`。  
完成后 **勿改** KB/DAG。
