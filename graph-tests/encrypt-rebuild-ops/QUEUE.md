# Encrypt/Decaps 重建 · 任务总队列

> 日志：`/mnt/workspace/encrypt-rebuild-npu-loop.log`

## 已关闭（Encrypt/Encaps）

| ID | 状态 |
|----|------|
| T01–T24 | 双绿；T24×30 不挂（**liboqs** Decaps 往返） |
| Q-ULT | answered（Encrypt/Encaps 设备路径） |

## 活跃（Decrypt/Decaps · 用户安心门禁）

> 用户卡死点：**Encaps↔Decaps 来回**。须设备 Decrypt + 设备 Decaps + 设备往返 NPU 压测。

| ID | 状态 |
|----|------|
| **Q-RT-HANG** | open：设备 Encaps↔Decaps 往返 NPU 不挂？ |
| **T25** | Decrypt 设备重建 · **本机编码中** |
| T26 | Decaps 设备（依赖 T25 + T22 Encaps） |
| T27 | 设备 Encaps→Decaps 往返 + NPU×N 压测 |

## NPU

| 轨 | 状态 |
|----|------|
| 当前 | T06 sticky；T25 CPU 绿后立刻推 Decrypt |
