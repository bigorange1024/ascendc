# graph-tests/enc_related

Encrypt **相关拼装**实验区（toys 结构维已穷尽 → 本区 dig 近生产体量）。

| 刀 | 任务书 / 目录 | 状态 |
|----|----------------|------|
| ER01 | [ER01-TASK.md](ER01-TASK.md) → [`ER01-encrypt-shaped-2launch-skel/`](ER01-encrypt-shaped-2launch-skel/) | **PASS**（CPU+SIM；sync_audit 红线 16→交 ER02） |
| ER02 | 清 ER01 的 cannbot **SYNC-02** 红线（禁否决） | **PASS**（就地修 ER01；红线 0） |
| ER03 | [ER03-TASK.md](ER03-TASK.md) → [`ER03-gate-realbrick-volume/`](ER03-gate-realbrick-volume/) | **PASS**（MAC 256×32；红线 0；X27） |
| ER04 | [ER04-TASK.md](ER04-TASK.md) → [`ER04-cube-ntt-volume/`](ER04-cube-ntt-volume/) | **PASS**（Cube×16；tick≈85889；X28） |
| ER05 | [ER05-TASK.md](ER05-TASK.md) → [`ER05-sticky-dual-compute/`](ER05-sticky-dual-compute/) | **PASS**（粘性×2；X29） |

**节奏**：旧 hang 线 `D-SIM-FRONTIER-PAUSE` 仍有效。  
**当前主线**已迁至 [`../enc_cann_ntt/`](../enc_cann_ntt/INDEX.md)（Host + 迁入 cann-ntt）。本目录 ER01–05 **只读教训**，不再叠同质 SIM 加压刀。

**方法（旧线）**：图谱 + hang-KB；cannbot sync-audit。  
**禁止**：抄旧 Encrypt；未跑 sync_audit 声称完成。
