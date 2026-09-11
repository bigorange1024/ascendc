# LAYOUT — RB-K07-pke-2launch

> 自 [`RB-K04-pke-full`](../RB-K04-pke-full/LAYOUT.md) 派生（PLAN [`LR-KG-F1`](../../launch-reduce-ops/tasks/LR-KG-F1/TASK.md)）：
> 把 K04 的 L2a（NTT）+ L2b（点积+编码）两个独立 MIX launch 融为**同一次** MIX launch 内的
> 两段串行握手，Host 侧从 3 launch 压到 **2 launch**。

## 拓扑（KB §B2）

| Launch | 核 basename | 类型 | 输入 | 输出 |
|--------|-------------|------|------|------|
| L1 | `kg_prep_custom.cpp`（链自 RB-K01） | AIV-only | `seed_d` | `Â`/`ŝ`/`ê` |
| mid-sync | Host | — | — | — |
| L2（融合） | `kg_ntt_dot_encode_custom.cpp` | MIX | `ŝ`/`ê`/`Â`+ζ/γ/ρ/mat | `ek_pke`/`dk_pke` |
| sync | Host | — | — | — |

L2 内部两段握手（同一次 launch，flag 1/3/4 复用两遍；永禁 5/7；AIC Wait 环禁 SyncAll；禁 SoftSync）：

```text
段1 AIC: Wait(4)→Wait(1)→Cube→Set(3)      段2 AIC: Wait(4)→Wait(1)→Cube→Set(3)
段1 AIV: Set(4)→Set(1)→Wait(3)→NTT(AIV0)  段2 AIV: Set(4)→Set(1)→Wait(3)→Dot+BE(AIV0)
```

段1 AIV0 的 `ComputeSeNtt` 把 ŝ̂/ê̂ **直接写到段2 期望的 S_NTT/E_NTT 地址**（`sNttOut`/`eNttOut`
参数直接传段2 子块内的对应偏移），无需额外 GM→GM 拷贝或 Host 中转；正确性由 AIV0 自身指令流的
程序顺序保证（先写完才会往下走到段2 的 Set(4)），不依赖任何忙等/软同步。

## 头文件隔离

- `k02_inc/`：段1（NTT）tiling / light_cube / ntt math（自 K02 契约复制，仅改 include）
- `k03_inc/`：段2（点积+编码）tiling / light_cube / dot_encode math（自 K03）；
  **本刀新增**：`namespace tiling` → `tiling_k03`，`struct TilingData` → `TilingDataK03Unused`
  （K04 是分文件各自 include，不会撞名；K07 融合进同一 TU 后两侧 `tiling.h` 会撞名，故重命名）。

## 融合 ws 布局

```text
ws = [ 段1(k02) tiling::wssize=20536B | 段2(k03) tiling_k03::wssize=35960B ]
```

- 段1 子块：`OFF_S`/`OFF_E`（Host 拷 L1 输出）、`OFF_ZETAS`/`OFF_MAT_A`/`OFF_MAT_B`（Host 读文件）、
  `OFF_S_NTT`/`OFF_E_NTT`（device 写，亦有诊断落盘）、`OFF_MAT_C`、`OFF_TRACE`。
- 段2 子块：`OFF_A_HAT`（Host 拷 L1 输出）、`OFF_S_NTT`/`OFF_E_NTT`（**device 段1桥接写，非 Host 预喂**）、
  `OFF_GAMMAS`/`OFF_RHO`/`OFF_MAT_A`/`OFF_MAT_B`（Host 读文件）、`OFF_T_HAT`/`OFF_EK`/`OFF_DK`（device 写）、
  `OFF_MAT_C`、`OFF_TRACE`。

## Host 胶水（相对 K04 的关键简化）

- L1→L2：Host 只需拷贝 `Â`/`ŝ`/`ê`（L1 输出）进融合 ws 的段1/段2 子块；读 `zetas`/`gammas`/`rho`/`mat_*`。
- **不再有** K04 里「L2a→L2b：Host 拷 ŝ̂/ê̂」这一步——段1→段2 的桥接完全在设备侧完成。

## Oracle

**权威** `scripts/liboqs_pke_ref keygen`（`SEED_D=20260619` Derand，`scripts/gen_data.py` 未改）；缺库 → BLOCKED。
