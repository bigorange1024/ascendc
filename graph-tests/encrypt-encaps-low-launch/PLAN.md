# PLAN · 少 launch · SIM 全实验

## 0. 用户锁定参数

| 路线 | Host `ACLRT_LAUNCH_KERNEL` 次数 | 运行态 |
|------|----------------------------------|--------|
| AIV NTT Encrypt / Encaps | **1** | cpu + sim |
| cann-ntt Encrypt / Encaps | **2** | cpu + sim |

## 1. AIV 线（W-A）

1. 审计 `AE-E-encrypt` / `AE-P-encaps`：SIM 路径仅 **1** 次 launch。  
2. 全量：`bash run.sh -r cpu` → `SIM_DIRECT=1 bash run.sh -r sim`（或用例等价入口）。  
3. 证据：log + STATUS；launch 次数写入审计表。

## 2. cann-ntt 线（W-C）

1. 现状 EN13/EP04 = **8** launch（积木接线）。  
2. **新建**少 launch 用例（勿在 EN13 上原地把 8 改成假绿）：  
   - **L1 prep**：采样 Â / CBD 等准备（1 launch）  
   - **L2 compute**：NTT(y)、matvec、dot、INTT、pack→c（1 launch）  
3. 设备内串级用核内同步；Host 中途仅 mid-sync（与历史 2-launch 外形一致）。  
4. cpu+SIM ≡ liboqs；统计 Host launch **必须 = 2**。

## 3. 禁令

- 本战役 **禁 `-r npu`**。  
- 禁止宣称 8-launch 为正确实现。  
- 不改 `examples/`（无 customspec 授权）。  
- 不擅自 git commit/push。
