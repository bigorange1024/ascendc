# 全局约束（每刀 TASK 默认继承）

## 角色

- **主控**：TASK/QUEUE/KB/DAG；**全部** Tailscale/SSH/`-r npu`；≤3min NPU 保活；**禁为等 SIM 停 NPU**（X6/X10）。  
- **Subagent**：只本机实现 + CPU/SIM；**禁止** SSH/NPU/读密钥/等审批；禁止空等「墙钟 20min 上板」。

## 永禁

1. 抄 / fork / 对照源码移植：  
   `pass-fix-f203-alg14*`、`alg20*`、`alg21*`、`examples/**/*encrypt*`、`*encaps*`、`*decaps*`、任何 `f203_encrypt_*` / `l18_l19` 实现文件。  
2. CrossCore **flag 5 / 7**；自造 SoftSync；AIC **Wait 等待环内** `SyncAll` / 阻塞 Sync。  
3. NTT S1–S3：**limbsplit**、**Gather**（见 KB §A2）。  
4. 改 `docs/notes/Encrypt-cannbot-rebuild-kb.md`、`docs/rg-encrypt-cannbot-rebuild.yaml`。  
5. `git commit` / `push` / 开分支（除非用户当次授权）。  
6. 并行多路 `run.sh -r sim`。  
7. 否决 `sync_audit` 红线（SIM 绿 ≠ 同步干净）。

## 允许只读

- `library/shared/**` 头与契约注释  
- 活跃分项探针 STATUS / INDEX（NTT、CBD、SampleNTT、Compress、ByteEncode/Decode_d、innerproduct、`pass-toy-mix-s123-byteencode-k2`）— **契约可参考，代码勿照抄大段**  
- 本仓 `run.sh` / CMake 壳惯例（如 `ascendc-tests/add_custom`、上述 toy 的工程壳）  
- cannbot：`thirdparty/cannbot-skills/ops/**`  
- **反卡死指导（必读再开 MIX 长链）**：[`docs/notes/MIX-Encrypt-Encaps-反卡死拓扑技术总结.md`](../../docs/notes/MIX-Encrypt-Encaps-反卡死拓扑技术总结.md) §5 检查单

## cannbot（编码刀强制）

写 CrossCore 前读：

```text
thirdparty/cannbot-skills/ops/ascendc-api-best-practices/references/api-crosscore-sync.md
```

编码完成后（有设备侧同步时）：

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  --check all --format json \
  <本刀代码目录> \
  > graph-tests/encrypt-rebuild-ops/tasks/<ID>/logs/sync_audit.json
```

红线原文（人工确认后）写入 `FEEDBACK.md`。挂死时读：`ops/ascendc-sync-audit/workflows/deadlock-triage.md`。

精度 skill：**骨架未通 / 仍挂前不启**。

## 验收默认（本战役：**NPU 优先**）

```bash
# Subagent 默认先交 CPU（编通 + I/O 壳）
cd <代码目录>
bash run.sh -r cpu -v Ascend910B4
# 长 SIM 非默认门禁；仅主控点名或排挂死时再跑：
# SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

- 主控：代码可编译即可 rsync **`-r npu`**；允许先红再修  
- 用例根无 stray `core*.dump` / `profile_*`  
- 墙钟：见各 TASK；超时 `blocked`，不傻等

## FEEDBACK 最短格式

```text
ID: PASS|FAIL|BLOCKED
cmd: …
exit: …
wall_min: …
sync_audit: clean|REDLINE(…)
sim: ok|hang|timeout|skip
notes: ≤8 行
next_hint: …
```

## NPU 波（仅主控）

- Subagent **不得**执行 Wave D。  
- 空闲断连约 **4 min** → 主控 ≤3 min 必有 SSH 活动。  
- `ASCEND_DEVICE_ID=0`；上板前清产物（X8）；禁脏杀。
