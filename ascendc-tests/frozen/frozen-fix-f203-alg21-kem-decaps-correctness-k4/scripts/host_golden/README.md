# host_golden — 分阶段期望（非生产路径）

**禁止** liboqs / `oqs.h`。允许：

1. **抄写**本仓库已有探针的 `*_ref.c` / `gen_data.py` 逻辑到本目录；
2. **拼装**多段 golden（G1 的 `a_hat`、G2 的 `r_hat`、…）；
3. 端到端 `golden_c.py` 仅在 **G4 设备全链就绪后** 启用。

默认 `run.sh` **不**生成本目录输出；仅 `ENCRYPT_VERIFY=1` 时 `verify_result.py` 读取 `output/golden_c.bin`。
