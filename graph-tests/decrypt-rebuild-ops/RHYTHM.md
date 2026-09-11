# Decrypt/Decaps 重建 · 节奏（RHYTHM）

| 轨 | 谁跑 | 说明 |
|----|------|------|
| **设计（S0）** | Subagent | 拓扑 / 刀序；不写核；不碰 NPU |
| **CPU 编码** | Subagent | 一刀一目录；CPU 绿 + sync_audit |
| **SIM** | Subagent（可选） | 任务书点名才跑；**非**默认门禁；慢则主控用 NPU 同实验顶上 |
| **NPU** | **主控 only** | 上板前 **先请用户启动云机**；再 `which_npu.sh` + `-r npu`；预算 180s；权威 liboqs；×N；**禁止**派给 subagent |

## 推进原则

1. **正确性**与**反卡死**同路径验收；禁止先堆功能再补同步。  
2. **云 NPU 默认关机**：需要时主控先跟用户说一声；未开 → `wait_npu`，继续 CPU/设计。  
3. **禁为等 SIM 停 NPU**：SIM 墙钟过长时，用户开机后主控直接 NPU 跑同一验收。  
4. Subagent FEEDBACK 写到 CPU/SIM 为止；主控在「主控批注」补 NPU 结果。
