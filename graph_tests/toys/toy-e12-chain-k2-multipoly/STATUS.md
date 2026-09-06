# STATUS — toy-e12-chain-k2-multipoly

| 项 | 值 |
|----|-----|
| task | E12 / D-exp-e12 |
| verdict | **PASS** |
| k | 2（两路 poly 串行 L2） |
| launch | 2-launch + SET(4) |
| Decompress_1(μ) | **保留**（两 poly 共享 μ） |
| 输出 | 256B = 2×ByteEncode_d(128B) |

## 验收（2026-09-06）

```bash
cd graph_tests/toys/toy-e12-chain-k2-multipoly
bash run.sh -r cpu -v Ascend910B4          # golden 0/256B
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4  # 3 轮不挂，golden 0/256B
```

| 模式 | 结果 | kernel wall |
|------|------|-------------|
| CPU ×3 | PASS | ~11s |
| SIM ×3 | PASS | ~206s（budget 1200） |

Golden：SHAKE 0/32；CBD 0/512；ByteEncode 0/256B。

## 几何摘要

- `src`：512 int32（2×CBD）
- `prf`：256B（2×128B）
- `g.bin`：512 int32（ĝ₀∥ĝ₁）
- `out/dst`：256B 拼接
- L2 工作区：`ws[W0]`、`ws[W1]` 各 256 int32

## 日志

- `/opt/cursor/artifacts/e12-cpu.log`
- `/opt/cursor/artifacts/e12-default-sim.log`
- `output/host_trace.log`
