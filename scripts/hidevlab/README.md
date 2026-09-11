# scripts/hidevlab — HiDevLab（昇腾在线开发）辅助

与 GitCode CANNLab（`scripts/cannlab/`）**分开维护**。

| 文件 | 作用 |
|------|------|
| [`webide_boot.sh`](webide_boot.sh) | **开机一次**：配 CANN/`LD_LIBRARY_PATH`/设备号、可选 `git pull`、体检、写 `/workspace/hidevlab_env.sh` |
| [`webide_recipe.sh`](webide_recipe.sh) | 打印可贴进 **WebIDE** 的真机配方（不 SSH） |

权威操作手册：[`docs/engineering/HiDevLab-WebIDE操作手册.md`](../../docs/engineering/HiDevLab-WebIDE操作手册.md)。
