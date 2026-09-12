# TASK-AE-FULL · 单 AIV 全向量 Encrypt + Encaps（SIM）

## 角色
你是**编码 subagent**。主控已落 W0（KB/图谱/QUEUE）。你负责从积木到 AE-E2/AE-P2 **写码+跑通 cpu+SIM**，直到 liboqs 交叉 max=0。

## 硬约束
- 路径：`graph-tests/aiv-kem-vector-sim/` 下新建子目录（可合并刀次，推荐最终落地 `AE-E-encrypt/` 与 `AE-P-encaps/`，中间积木可放 `bricks/`）
- **ML-KEM-1024**：k=4,n=256,q=3329,du=11,dv=5,η1=η2=2
- **KERNEL_TYPE_AIV_ONLY**，单用例 **blockDim=1**，Encrypt/Encaps **单 launch**
- **禁 Cube / CrossCore / GATE 胖 MIX**
- 底层 NTT/INTT/算术/压缩/编解码 **至少用到 Vector API**（Mul/Muls/Add/Gather 等）；禁止整段纯标量扫 256 当主路径
- NTT 基线：复用/迁入 `graph-tests/aiv_ntt/AV01-single-aiv-mlkem-ntt/` 的 Gather 蝶形思路与 `generated/` 根表（**不要**抄 enc_cann_ntt / examples 全核）
- **AV01 无 INTT**：必须自建与正向互逆的向量 INTT（含 n^{-1} 与正确输出序，使 NTT∘INTT / Encrypt 对拍 liboqs）
- 运行：`bash run.sh -r cpu -v Ascend910B4` 与 `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`；**禁 -r npu**
- 每刀/每核：`python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py ...`
- **禁止** `git checkout -b` / `git commit` / `git push`
- 中文注释（文件头+函数头+关键块）
- golden：**禁止**与设备同源假绿；权威 = **liboqs**（或仓内 `library/shared/f203_kem_ref`+已验证 PKE encrypt 路径）；先验证 Host 参考 ≡ liboqs 再接线

## 推荐实现策略（可调整，但出口不变）
1. Host：用 liboqs/`f203_kem_ref` 固定 (ek,m,coins)→c 与 Encaps→(c,K)
2. 积木（可同一目录迭代）：
   - polyvec NTT（4× AV01 风格，同 launch 内循环）
   - 向量 INTT（新建表+核）
   - NTT 域 matvec Âᵀ∘ŷ 与 dot ⟨t̂,ŷ⟩（Vec Mul/Add + Barrett/Mont 模约）
   - Compress_du/dv + ByteEncode + pack 1568B
   - Sample：可先 Host 采样喂 GM（仍单 launch 做变换），但最终 AE-E2 输入必须是 **ek|m|coins**（或 Encaps 的 ek|m）与 liboqs 同契约；采样在 Host 或 Device 均可，**只要 c 一致**。优先 Device SHAKE 若时间允许，否则 Host 采样+Device 变换也可过 AE-E2（须在 STATUS 写明边界）；Encaps 的 H/G 同理可先 Host 再并入。
3. **AE-E-encrypt**：单 AIV 单 launch 输出 `c`；cpu+SIM；`c` vs liboqs max=0
4. **AE-P-encaps**：单 AIV 单 launch 输出 `c,K`（或 Host FO + 真调 Encrypt 同进程两次 launch **不达标**——Encaps 也须能单 launch；若 FO 在 Host 则设备 launch 仍是 Encrypt-only，须在 STATUS 标 DEFERRED_HOST_FO，但 **Encrypt 核**须单 launch；主控接受 Encaps = Host H/G + 一次 Encrypt launch 作为 AE-P2 弱完成，**强完成**为 FO 也在同 AIV launch。请优先强完成；若 SIM 超时/UB 不够，先弱完成并写明，再挤强完成。）

## 壳
对齐 `graph-tests/bricks/RB-T05-*` 或 AV01 的 CMake/run.sh（cpu/sim KernelLaunch）。

## 交付物（回收）
1. 各目录 `STATUS.md`（命令、tick、max diff、sync_audit 红线）
2. 更新 `graph-tests/aiv-kem-vector-sim/QUEUE.md` 状态
3. 日志拷到 `/opt/cursor/artifacts/aiv-kem-vector-sim/`
4. 回报：是否 AE-E2 / AE-P2 PASS；阻塞点；UB/tick 数字

## 验收口令
- AE-E2 PASS ⇔ cpu+SIM 下 Encrypt `c`≡liboqs
- AE-P2 PASS ⇔ cpu+SIM 下 Encaps `c`与`K`≡liboqs（弱/强完成须写明）
