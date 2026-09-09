# KeyGen 重建 · 自治执行队列

> 刷新：2026-09-09（K02 PASS_CPU；**待开机 NPU×30**）

| 序 | ID | 状态 | 说明 |
|----|-----|------|------|
| 0–1 | DOC / S0 | **done** | |
| 2–4 | KGR-P01…P03 | **done** | PASS_CPU |
| 5 | KGR-P04 | **done_cpu** | liboqs_pke；**npu wait** |
| 6 | KGR-K01 | **done_cpu** | kem_tail；**npu wait** |
| 7 | KGR-K02 | **done_cpu** | 四 launch ≡liboqs_kem；**npu wait** |
| 8 | NPU 加压 | **blocked_cloud** | P04/K01/K02（及砖）干净卡 ×30；关 `Q-KEYGEN-HANG` |

CPU 全链已绿。下一刀：**请用户启动云机** → 主控上板 NPU×30。
