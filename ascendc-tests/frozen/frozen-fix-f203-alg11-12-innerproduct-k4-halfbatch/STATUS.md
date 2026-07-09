# frozen-fix-f203-alg11-12-innerproduct-k4-halfbatch

**状态**：⛔ **已冻结**（2026-06-17）  
**判决书**：[FROZEN.md](FROZEN.md)

二期 half 批处理路线快照。活跃继任：[pass-fix-f203-alg11-12-innerproduct-k4](../../pass-fix-f203-alg11-12-innerproduct-k4/)（仅一期全 poly）。

| 路径 | 正确性（冻结时） | SIM tick（OPTS=1） |
|------|------------------|-------------------|
| 二期 + aBlockQue | ✓ | ~23220 |
| 一期全 poly（继任） | ✓ | ~23248 |

**禁止**在本目录执行 `run.sh` 或 CI。
