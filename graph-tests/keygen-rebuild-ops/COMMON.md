# 全局约束（KeyGen 每刀 TASK 默认继承）

## 角色与分流

- **主控**：TASK/QUEUE/KB/DAG；**全部** NPU/SSH；上板前请用户开机；刀间禁空心跳；目标未达不停手。  
- **有云机后**：写码/排错默认跟 **NPU**。  
- **加压**：中间 **×30**；算子级 **×100**；预算 **180s**。  
- **Subagent**：设计 / 编码 / CPU / SIM；**禁止** SSH/NPU。  
- **可读**：[`ascendc-engineering-kb.md`](../../docs/notes/ascendc-engineering-kb.md) / [`rg-ascendc-engineering.yaml`](../../docs/rg-ascendc-engineering.yaml) / inventory / 本 COMMON / 反卡死 note。  
- **禁改**：工程 KB、`docs/rg-ascendc-engineering.yaml`（及已归档的战役 kb/yaml 指针）。

## 永禁

1. 抄 / fork / 对照源码移植 **KeyGen 算子级**代码：  
   `examples/**/*keygen*`、`pass-fix-f203-alg13*keygen*`、`pass-fix-f203-alg19*keygen*`、  
   `f203_keygen_*`、KeyGen 树内 `mmad_custom.cpp`、`**/frozen/**` KeyGen 源码。  
2. 整文件 fork Encaps/Decrypt 重建核改名冒充 KeyGen（契约/同步模式可参考）。  
3. CrossCore **flag 5 / 7**；SoftSync；AIC Wait 环内 `SyncAll`。  
4. NTT S1–S3：**limbsplit**、**Gather**。  
5. 同 binary 内 kernel `.cpp` **basename 撞名**。  
6. `GlobalTensor::SetValue` 写业务 GM。  
7. 未经授权 `git commit` / `push` / 开分支。  
8. 并行多路 `run.sh -r sim`。  
9. 否决 `sync_audit`；用 python 冒充 liboqs。

## 允许只读

- `library/shared/**`  
- `graph-tests/toys|bricks/**`；`enc_related/RB-T22…`、`dec_related/RB-D*` 的 FEEDBACK/LAYOUT  
- 分项探针 STATUS（alg7/8、2s1e NTT、alg11-12、byteencode）— **契约可参考，禁大段照抄**  
- `docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md`（契约）  
- `docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`（反例）  
- `docs/notes/MIX-Encrypt-Encaps-反卡死拓扑技术总结.md`  
- cannbot：`thirdparty/cannbot-skills/ops/**`

## cannbot（编码刀强制）

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  --check all --format json \
  <本刀代码目录> \
  > graph-tests/keygen-rebuild-ops/tasks/<ID>/logs/sync_audit.json
```

## 验收默认

```bash
# Subagent
cd <代码目录>
bash run.sh -r cpu -v Ascend910B4

# 主控 NPU（subagent 禁止）
# bash run.sh -r npu -v Ascend910B3
```

TASK 须标明 `runner: subagent_cpu_sim | main_npu`。

## FEEDBACK 最短格式

```text
ID: PASS|FAIL|BLOCKED|DESIGN_OK
cmd: …
exit: …
wall_min: …
sync_audit: n/a|clean|REDLINE(…)
notes: ≤12 行
next_hint: …
```
