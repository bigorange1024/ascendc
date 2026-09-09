# 全局约束（Decrypt/Decaps 每刀 TASK 默认继承）

## 角色与分流

- **主控**：TASK/QUEUE/KB/DAG；**全部** Tailscale/SSH/`-r npu` / 真机 liboqs / ×N；**上板前先请用户启动云 NPU**（默认未开）；**禁为等 SIM/本机 CPU 停 NPU**——卡空闲即可与 subagent **并行上板**；占机用心跳、放机停心跳（见 work-mode）。  
- **有云机后**：写码/排错默认跟 **NPU**；勿以 CPU 孪生为唯一开发基线（易引入仅 NPU 才暴露的编译/同步错）。  
- **目标未达不停手**：作业结束立刻下一刀或沉淀 KB/DAG/notes；禁止无故停等。  
- **加压**：中间用例 **×30**；最终算子 **×100**。  
- **Subagent**：本机 **设计 / 编码 / CPU / SIM**；**禁止** SSH/NPU/读云密钥/空等上板授权。云机已开时主控应并行推 NPU，勿等 subagent CPU 空转。  
- **可读**：Decrypt KB / DAG / inventory / 本 COMMON / 反卡死 note / cannbot ops / toys·bricks STATUS。  
- **禁改**：`docs/notes/Decrypt-cannbot-rebuild-kb.md`、`docs/rg-decrypt-cannbot-rebuild.yaml`、Encrypt KB/DAG。

## 永禁

1. 抄 / fork / 对照源码移植：  
   `pass-fix-f203-alg15*`、`alg21*`、`examples/**/*decrypt*`、`*kem-decaps*`、  
   `graph-tests/enc_related/RB-T25|T26|T27-*/` **实现文件**、`**/frozen/**` Decrypt/Decaps 源码、  
   任意 `f203_decrypt_*` / `f203_kem_dec_*` / `l18_l19` 实现。  
2. CrossCore **flag 5 / 7**；自造 SoftSync；AIC **Wait 环内** `SyncAll`。  
3. NTT S1–S3：**limbsplit**、**Gather**。  
4. 同 binary 内 kernel `.cpp` **basename 撞名**。  
5. `GlobalTensor::SetValue` 写业务 GM（X12）。  
6. `git commit` / `push` / 开分支（除非用户当次授权）。  
7. 并行多路 `run.sh -r sim`。  
8. 否决 `sync_audit` 红线；用 python 冒充 liboqs 权威。

## 允许只读

- `library/shared/**`  
- `graph-tests/toys|bricks/**` STATUS/LAYOUT；`enc_related/RB-T22…T24` FEEDBACK（Encaps 思路）  
- 活跃分项探针 STATUS（NTT、CBD、Compress、ByteEncode/Decode、innerproduct、toy-mix）— **契约可参考，禁大段照抄**  
- `docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md`（原理）  
- `docs/notes/MIX-Encrypt-Encaps-反卡死拓扑技术总结.md` §5  
- cannbot：`thirdparty/cannbot-skills/ops/**`

## cannbot（编码刀强制；设计刀建议读）

```text
thirdparty/cannbot-skills/ops/ascendc-api-best-practices/references/api-crosscore-sync.md
thirdparty/cannbot-skills/ops/ascendc-sync-audit/SKILL.md
thirdparty/cannbot-skills/ops/ascendc-sync-audit/workflows/deadlock-triage.md
```

编码完成后：

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  --check all --format json \
  <本刀代码目录> \
  > graph-tests/decrypt-rebuild-ops/tasks/<ID>/logs/sync_audit.json
```

精度 skill：**骨架未通 / 仍挂前不启**。

## 验收默认

```bash
# —— Subagent ——
cd <代码目录>
bash run.sh -r cpu -v Ascend910B4
# 仅当 TASK 点名：
# SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# —— 主控（NPU；subagent 禁止执行）——
# eval "$(bash scripts/cannlab/which_npu.sh --export)"
# bash run.sh -r npu -v Ascend910B3   # 预算 180s
```

TASK 须标明 `runner: subagent_cpu_sim | main_npu`。若写了 `-r npu` 却派给 subagent → **违规，主控收回**。

## FEEDBACK 最短格式

```text
ID: PASS|FAIL|BLOCKED|DESIGN_OK
cmd: …（设计刀可写 review）
exit: …
wall_min: …
sync_audit: n/a|clean|REDLINE(…)
notes: ≤12 行
next_hint: …
```
