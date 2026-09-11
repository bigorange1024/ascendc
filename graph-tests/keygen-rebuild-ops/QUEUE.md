# KeyGen 重建 · 自治执行队列

> 刷新：2026-09-09（**NPU×30 三档全绿**；关 `Q-KEYGEN-HANG` / `Q-KEYGEN-CORRECT`）

| 序 | ID | 状态 | 说明 |
|----|-----|------|------|
| 0–1 | DOC / S0 | **done** | |
| 2–4 | KGR-P01…P03 | **done** | PASS_CPU |
| 5 | KGR-P04 | **done** | liboqs_pke；**NPU×30 pass=30** |
| 6 | KGR-K01 | **done** | kem_tail；**NPU×30 pass=30**（含 `__gm__` 字面量修复） |
| 7 | KGR-K02 | **done** | 四 launch ≡liboqs_kem；**NPU×30 pass=30** |
| 8 | NPU 加压 | **done** | 干净卡 `cannlab-npu`；预算 180s；fail=0 hang=0 |

CPU 全链 + **NPU×30 反卡死/正确** 已齐。战役关闸完成（incubating 级）。  
下一：用户若要晋级 `examples/stable-*` 须另开 `#交付#`。
