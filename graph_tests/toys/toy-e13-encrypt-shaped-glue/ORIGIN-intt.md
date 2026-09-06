# ORIGIN — INTT（E04 同系 / ntt256 矩阵逆）

| 项 | 内容 |
|----|------|
| 路线 | **E04/E06 同系**：`pass-merged-kyber-mix-ntt256` Split→AIC Mmad→Merge |
| 逆变换 | Host 预计算 `Minv = M^{-1} (mod q)` → `Minv4.bin`；设备再跑一遍同系 Cube/Merge |
| 语义 | **≠ Tag5T**（非 `pass-fix-f203-stage123-ntt-intt-polyvec8-vec` LUT/polyvec8） |
| 任务允许 | TASK-E07：可用 E04 同系 INTT；STATUS 须写明 ≠ Tag5T |
| 未采用 | 整图拷贝 Tag5T polyvec8 INTT（与 ntt256 数据面不兼容）；抄 Encrypt |

参考只读（未改原目录）：`ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/`（Tag5T 对照语义）。
