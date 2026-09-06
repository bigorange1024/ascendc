# Encrypt 卡死 — 实机操作卡（照着做）

> 反馈：**只能打字**，不要传任何文件。

## 测什么

一次脚本自动跑 **N0–N10**（不用手点）：

| ID | 测什么 | 卡 |
|----|--------|----|
| N0 | PKE KeyGen（对照，应不易挂） | 1 |
| N1 | T01 最短 MIX 握手 | 3 |
| N2 | T02 生产 GATE 时序 | 3 |
| N3 | T03 全 FSM | 3 |
| N4 | T04 假循环加压 | 3 |
| N5 | T05 两次 launch | 3 |
| N6 | T06 GATE 真算 | 3 |
| N7 | T07 采样后再 FSM | 3 |
| N8 | **PKE Encrypt + TRACE**（主嫌疑） | 1 |
| N9 | KEM Encaps + TRACE | 1 |
| N10 | PKE Decrypt（对照） | 1 |

目的：看卡死出在哪一档、屏幕上最后出现哪些编号。

## 怎么测

```bash
git fetch && git checkout cursor/cann-ntt-operator-refactor-fe53 && git pull
cd <仓库根>
unset ASCEND_DEVICE_ID
export SOC_VERSION=Ascend910B4
bash scripts/npu_hang_rewrite_one_trip.sh
```

等它跑完（单例挂了会超时并继续下一项）。

## 怎么反馈（只打字）

终端末尾有 **TYPE_BACK**。整段贴回聊天，可手改。格式：

```text
N0 通
N1 通 101 201 301 401 402 203 303 199
N8 超时 0 1 2 3
N9 挂 空
```

- 状态：`通` / `挂` / `超时` / `编译失败`
- toys：抄屏幕十进制编号
- N8/N9：抄 `[l18-trace]` 后面的下标；没有就写 `空`

**不要**传 tar、日志、截图、md。
