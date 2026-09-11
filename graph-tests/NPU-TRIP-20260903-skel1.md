# 偶发上机清单 · toy skel1（DataCopy TRACE）

> 对应图谱决策：`D-NPU-TRIP-TOY-SKEL1`  
> 自检依据：[`INDEX.md`](INDEX.md) §1.1  
> **未获用户同意改 stable 前**：不上机改生产 `FusedTraceMark`；本清单只跑 toy。

## §1.1 勾选

| # | 项 | 状态 |
|---|----|------|
| 1 | 假设有节点；SIM 证据已写 | ✅ `Q-EMPTY-TRACE` / `J-EMPTY-TRACE-*`；GT-1..4 PASS；`F-TOY-TRACE-DATACOPY-PASS` |
| 2 | 失败对照已沉 | ✅ `J-FAIL-AIC-SCALAR-TRACE`（标量假空）；sticky SyncAll/INTT5/7/拆双 Cube 等已在图 |
| 3 | 上机步骤 ≤ 半页 | ✅ 见下 |
| 4 | 改动集小、可单独同步 | ✅ 仅 `ascendc-tests/pass-toy-encrypt-fsm-l18-skel1/`（含 DataCopy TRACE） |
| 5 | 失败后先刷图再下一刀 | ✅ 用户带回日志 → 主控刷新 → 再设计 |

## 上机步骤（拷到有卡机）

```bash
# 目录
cd ascendc-tests/pass-toy-encrypt-fsm-l18-skel1

# 强制重编（防旧二进制；见 F-SKIP-REBUILD-OLD-FUSED）
FORCE_REBUILD=1 bash run.sh -r npu -v Ascend910B4
# 若该用例 run.sh 无 -r npu：按本机惯例编 install 后直接跑可执行，并设 ASCEND_DEVICE_ID 按树分卡（tests=3）

# 期望
# - 不卡 SynchronizeStream；或卡死时记下挂点
# - 打印 [toy-l18-trace] stages set=… 含 AIV0 与 AIC 非空槽（SIM 参照：0 1 2 3 5 6）
# - 若 stages 0/8 全空 → 支持 J-EMPTY-TRACE-AIV0（首 mark 前挂）

# 可选对照（同卡、另一次）
# KeyGen 短冒烟 → 服务 Q-KEYGEN-CONTRAST
```

## 请带回

1. 完整终端日志（含 TRACE 行、exit、是否超时）  
2. 卡死则：挂多久、是否 reset 后复现、device id  
3. 若有 `core*.dump` / OPPROF：路径说明即可（勿整包塞聊天）

## 明确不做

- 不以本趟绿代替 Encrypt/`l18_l19` 消粘验收（`Q-ULT` 仍 open）  
- 不自主改 `examples/stable/**/f203_encrypt_l18_l19_kernel.cpp` 的 `FusedTraceMark`（须另确认）  
- 不连环催第二次拷机
