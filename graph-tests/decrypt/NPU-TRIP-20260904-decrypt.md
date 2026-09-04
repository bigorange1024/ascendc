# 实机测试 · 极简（只打字反馈）

> 借机有访问控制：**不要**存日志、不要打包、不要 `tee`。  
> 你反馈时**只打几行字**给我即可。

分支：`cursor/cann-ntt-operator-refactor-fe53`

---

## 你跑什么（最多两步）

### 1）正式 Decrypt + TRACE（核心；优先只做这一步）

```bash
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4
export ASCEND_DEVICE_ID=1
FORCE_REBUILD=1 F203_DECRYPT_TRACE=1 SEED_D=20260619 bash run.sh -r npu -v Ascend910B4
```

`ASCEND_DEVICE_ID=1` = 用 1 号那张 NPU（正式算子习惯用这张）。

### 2）仅当第 1 步「秒挂 / 完全起不来」时，再跑玩具对照

```bash
cd ascendc-tests/pass-toy-decrypt-fsm-fused-skel1
export ASCEND_DEVICE_ID=3
FORCE_REBUILD=1 TOY_LAUNCH_REPEAT=2 bash run.sh -r npu -v Ascend910B4
```

---

## 你打字回我什么（照抄模板，能填多少填多少）

```
卡号:
结果: 过 / 挂 / 超时124 / 编译失败
大概耗时:
若挂: 最后一条 [decrypt-trace] 整行（有 softSync= 更好）
若过: 有没有 PASS/SUCCESS
备注: （一句即可）
```

**不要**：日志文件、截整屏、OPPROF、第二遍同卡连环重跑。

挂了就停；过了也可以只回「卡1 过 PASS」。
