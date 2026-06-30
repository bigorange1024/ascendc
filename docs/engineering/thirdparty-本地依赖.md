# thirdparty/ — 本地依赖（不进 Git 仓）

`thirdparty/` 整树在根 `.gitignore` 中排除，**不会**随 `git push` / `git pull` 同步。Agent 与协作者须在本机自行维护，**禁止删除或 `git clean -fdx` 后不重装**。

## 期望目录

```text
thirdparty/
├── ntt_study/       # 研究线 golden / LUT 对照（本机 tree）
├── merged_kyber/    # Kyber MIX FSM 参考（本机 tree）
├── tiny_sha3/       # Host SHA3/SHAKE golden
└── liboqs/          # ML-KEM KAT（tag 0.15.0，需 build）
```

## 安装

### tiny_sha3（公开 upstream）

```bash
git clone --depth 1 https://github.com/mjosaarinen/tiny_sha3.git thirdparty/tiny_sha3
```

上游：[mjosaarinen/tiny_sha3](https://github.com/mjosaarinen/tiny_sha3)（`sha3.c` / `sha3.h`；本仓路径恒为 `thirdparty/tiny_sha3`）。

### ntt_study / merged_kyber

无公开 clone URL；从办公室机器、`~/ntt_study` 或备份拷贝/软链到 `thirdparty/`。

### liboqs

见 `qa/2026-06/2026-06-08-Rule-Skill落地与FIPS203-204终极目标.md` §liboqs（tag **0.15.0**）。

## Agent 约束

| 禁止 | 说明 |
|------|------|
| `git add thirdparty/` | 体积大、非 ascendc 交付物 |
| 删空 `thirdparty/` 子目录 | 会导致 shake/KAT/对照脚本失败 |
| `git clean -fdx` 后不恢复 | 等同删库 |

## Zone.Identifier

从 Windows 拷贝可能产生 `*:Zone.Identifier`；已加入 `.gitignore`。发现时用：

```bash
find . -name '*:Zone.Identifier' -delete
find . -name '*.Identifier' -delete
```
