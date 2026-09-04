# 实机测试 · 极简（只打字反馈）

> 借机有访问控制：**不要**存日志、不要打包。反馈只打几行字。  
> 分支：`cursor/cann-ntt-operator-refactor-fe53`

**顺序不变：先 toy，再正式 Decrypt。**  
（SIM 上 toy 没挂 ≠ 真机没挂；先确认握手骨架在 NPU 上是否挂，再跑生产路径。）

---

## 1）Decrypt toy（必做）

```bash
cd ascendc-tests/pass-toy-decrypt-fsm-fused-skel1
export ASCEND_DEVICE_ID=3
FORCE_REBUILD=1 TOY_LAUNCH_REPEAT=2 bash run.sh -r npu -v Ascend910B4
```

`ASCEND_DEVICE_ID=3` = 探针/toy 用这张卡。

## 2）正式 Decrypt + TRACE（必做；1 过了再跑）

```bash
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4
export ASCEND_DEVICE_ID=1
FORCE_REBUILD=1 F203_DECRYPT_TRACE=1 SEED_D=20260619 bash run.sh -r npu -v Ascend910B4
```

`ASCEND_DEVICE_ID=1` = 正式算子用这张卡。  
**若 1 已挂：停，不要硬跑 2。**

## 可选）Encrypt toy（时间够、且 1 没挂再跑）

```bash
cd ascendc-tests/pass-toy-encrypt-fsm-l18-skel1
export ASCEND_DEVICE_ID=3
FORCE_REBUILD=1 TOY_LAUNCH_REPEAT=2 bash run.sh -r npu -v Ascend910B4
```

---

## 打字回我（照抄，能填多少填多少）

```
1 toy: 卡3 / 过|挂|超时124|编译失败 / 一句现象
2 decrypt: 卡1 / 过|挂|超时124|编译失败 /
   若挂: 最后一条 [decrypt-trace] 整行
   若过: 有没有 PASS
（可选）encrypt toy: …
```

不要日志文件；挂了就停，别同卡连环重跑。
