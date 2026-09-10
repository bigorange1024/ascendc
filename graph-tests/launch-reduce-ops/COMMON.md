# Launch 压缩 · COMMON（覆盖旧重建「先 CPU」）

继承工程 KB + KeyGen/Decrypt 永禁；本战役增量如下。

## 验收

- **唯一结案 runner**：`bash run.sh -r npu …`（真机号以 `which_npu` 为准）。  
- **禁止**新写 CPU/SIM 测试路径作门禁；既有 `run.sh` 可保留但 **Agent 不得跑 `-r cpu|-r sim` 结案**。  
- 加压：中间 ×30；预算 180s；干净卡。

## cannbot

每刀设备码变更后必跑 sync_audit（见 PLAN §6）；日志进 `tasks/<ID>/logs/`。

## Git

无用户明确「提交/推送」→ 禁止 commit/push。

## 空闲

NPU keepalive 期间刀间空转 **&lt;3 分钟**；否则停心跳放机。

## 禁抄

stable / pass-fix / frozen 的 KeyGen、Decrypt fused、Decaps 核：**禁整树 fork**。  
允许：只读 launch 数与「prep+单 MIX」**模式**；从 `RB-K04`/`RB-D04`/`RB-T26` **重建树**改。
