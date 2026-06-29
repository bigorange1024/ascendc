#!/usr/bin/env python3
# @probe stable-mlkem-f203-pke-keygen-k4
# @file scripts/inject_probe_code_comments.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `inject_probe_code_comments.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。

# -*- coding: utf-8 -*-
"""
一次性/可重复：为探针源码树 prepend @probe 注释块（已含 marker 的文件跳过）。
从探针根目录运行: python3 scripts/inject_probe_code_comments.py
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

PROBE_MARKER_NAME = "stable-mlkem-f203-pke-keygen-k4"
MARKER = f"@probe {PROBE_MARKER_NAME}"

SOURCE_SUFFIXES = {".cpp", ".h", ".hpp", ".cmake", ".sh", ".py"}

EXCLUDE_DIR_PREFIXES = (
    "build",
    "out",
    "sim_log",
    "cceprint",
    "npuchk",
    "thirdparty",
)

PRODUCTION_IO_DEFAULT = (
    "默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；"
    "output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。"
    " / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps."
)

LAUNCH_PREP = "prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）"
LAUNCH_MMAD = "mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）"
LAUNCH_NA = "N/A（host / 脚本 / CMake 不参与 device launch）"

AI_CORE_PREP = (
    "SIM 剖面：prep 段 0×AIC + 2×AIV；双 AIV 并行 BuildAHat16ShardWithUb（blockIdx 0/1 各 8 poly）；"
    "block0 独占 PRF+CBD；CPU [SUCCESS] 中 AIC_* 为 tikicpu 伪影，以 profile_subtask_log*.toml 为准。"
)
AI_CORE_MMAD = (
    "SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。"
)
AI_CORE_NA = "N/A（非 AI Core 内核源）"

SUBDIR_ROLE_PREFIX = {
    "prep/ahat": (
        "prep/ahat：设备侧生成矩阵 A_hat（FIPS203 Alg.6/布局 f203_a_hat16）；"
        "AIV-only UB 流水，为 compute MMAD 提供 a_hat GM。"
        " / Device A_hat generation for keygen prep."
    ),
    "prep/alg7": (
        "prep/alg7：FIPS203 Algorithm 7 SampleNTT（CBD+rej 采样 ŝ/ê 等）；"
        "含 XOF、rej 标量/向量与 compact LUT ROM。"
        " / Alg.7 SampleNTT rejection-sampling prep kernels."
    ),
    "prep/alg8": (
        "prep/alg8：η=2 centered binomial（secret/noise）设备采样与 LUT；"
        "与 presample/alg7 链接成 prep 链。"
        " / Alg.8 η=2 CBD prep helpers."
    ),
    "prep/presample": (
        "prep/presample：SHAKE/PRF/CBD 预采样与 NTT17 链入口；"
        "从 seed 派生设备侧中间量供 alg7/alg8/ahat。"
        " / Presample + Keccak/PRF device vector entry."
    ),
    "compute/": (
        "compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；"
        "第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。"
        " / Full keygen compute (mmad_custom) sources."
    ),
}

FILE_ROLE_OVERRIDES: dict[str, str] = {
    "run.sh": (
        "探针主编排：gen_data → 编译 prep+compute+keygen → 两次 kernel launch → 生产 output 对拍。"
        " / Orchestrates full Alg.13 keygen probe."
    ),
    "main_keygen.cpp": (
        "全链 keygen host：串联 prep launch 与 compute launch，管理 GM 与生产 I/O。"
        " / Full-chain keygen host driver."
    ),
    "main_keygen_prep.cpp": "prep 子工程 host 入口（单独调试 prep launch）。 / Prep-only host main.",
    "main_compute.cpp": "compute 子工程 host 入口（mmad_custom + 生产 ek_pke 路径）。 / Compute-only host main.",
    "main.cpp": "legacy/staged host 入口或旧编排；生产以 main_keygen + run.sh 为准。 / Legacy host entry.",
    "f203_keygen_prep_entry.cpp": "prep 设备 kernel 统一 entry（注册 f203_keygen_prep）。 / Prep device entry TU.",
    "f203_keygen_ek_append_entry.cpp": "ek_pke 追加/融合 launch 入口（与 keygen 链衔接）。 / ek_pke append entry.",
    "compute/main.cpp": "compute 包 host：读 bin、launch mmad_custom、写 dst/ek/sk。 / Compute package host.",
    "compute/mmad_custom.cpp": "mmad_custom 设备主核：AIC MMAD + AIV NTT/Alg11/行18–20。 / Primary device kernel.",
    "scripts/keygen_golden.py": "Host golden：与设备生产 I/O 对齐的 ek_pke/dk_pke 期望。 / Production golden generator.",
    "scripts/gen_data.py": "根级 gen_data：准备 seed/LUT 与可选 debug golden。 / Top-level input generator.",
    "scripts/prepare_production_input.py": "从 seed 生成生产 input/（seed_d + stacked LUT）。 / Production input prep.",
    "scripts/verify_production.py": "对拍 output/ek_pke+dk_pke 与 golden（生产验收）。 / Production verify.",
    "scripts/kat_liboqs_vs_ascendc.py": "liboqs KAT 与 AscendC 输出对比（KEYGEN_KAT=1 静默路径）。 / liboqs KAT harness.",
    "kat_liboqs_vs_ascendc.sh": "KAT 外壳脚本：调用 liboqs 与 run.sh。 / KAT wrapper shell.",
    "kat_liboqs_staged.sh": "分阶段 KAT（legacy staging I/O）。 / Staged KAT (legacy I/O).",
}


def probe_root_from_script() -> Path:
    return Path(__file__).resolve().parent.parent


def path_excluded(rel_parts: tuple[str, ...]) -> bool:
    for part in rel_parts:
        for pref in EXCLUDE_DIR_PREFIXES:
            if part == pref or part.startswith(pref):
                return True
    return False


def comment_prefix(suffix: str) -> str:
    if suffix in {".cpp", ".h", ".hpp"}:
        return "//"
    return "#"


def layer_for(rel_posix: str) -> str:
    if rel_posix.startswith("prep/"):
        return "prep"
    if rel_posix.startswith("compute/"):
        return "compute"
    if rel_posix.startswith("cmake/"):
        return "cmake"
    if rel_posix.startswith("scripts/"):
        return "script"
    if rel_posix in {"kat_liboqs_staged.sh", "kat_liboqs_vs_ascendc.sh", "main.cpp"}:
        return "legacy"
    if rel_posix.endswith(".sh") or rel_posix.startswith("main") or rel_posix.endswith("_entry.cpp"):
        return "host"
    if "prep" in rel_posix and rel_posix.endswith(".cpp"):
        return "prep"
    return "host"


def launch_for(layer: str, rel_posix: str) -> str:
    if layer == "prep" or rel_posix.startswith("prep/") or "prep_entry" in rel_posix:
        return LAUNCH_PREP
    if layer == "compute" or rel_posix.startswith("compute/"):
        return LAUNCH_MMAD
    return LAUNCH_NA


def ai_core_for(layer: str, rel_posix: str) -> str:
    if layer == "prep" or rel_posix.startswith("prep/") or "prep_entry" in rel_posix or "prep_ub" in rel_posix:
        return AI_CORE_PREP
    if layer == "compute" or rel_posix.startswith("compute/"):
        return AI_CORE_MMAD
    return AI_CORE_NA


def production_io_for(rel_posix: str) -> str:
    legacy_hints = ("kat_liboqs_staged", "staged", "legacy", "main.cpp")
    if any(h in rel_posix for h in legacy_hints):
        return (
            PRODUCTION_IO_DEFAULT
            + " 本文件可能用于 legacy/staged I/O 或分阶段调试，非默认生产路径。"
            " / May use legacy staging I/O."
        )
    if rel_posix.startswith("scripts/compute/") or rel_posix == "main_compute.cpp":
        return PRODUCTION_IO_DEFAULT + " compute 子树可单独跑中间 bin（调试）。 / Compute subtree debug bins optional."
    return PRODUCTION_IO_DEFAULT


def role_for(rel_posix: str, path: Path) -> str:
    if rel_posix in FILE_ROLE_OVERRIDES:
        return FILE_ROLE_OVERRIDES[rel_posix]
    for prefix, text in SUBDIR_ROLE_PREFIX.items():
        if rel_posix.startswith(prefix):
            stem = path.stem
            return f"{text} 本文件 `{path.name}` 为该子模块组件。 / Component: {path.name}."
    stem = path.stem
    if rel_posix.startswith("cmake/"):
        return (
            f"CMake 片段：编译/链接 prep、compute 或 keygen 目标。"
            f" / Build wiring for `{path.name}`."
        )
    if rel_posix.startswith("scripts/"):
        return f"探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `{path.name}`."
    if rel_posix.endswith("_entry.cpp"):
        return f"设备 kernel entry 翻译单元 `{path.name}`。 / Device entry `{path.name}`."
    if rel_posix.endswith((".hpp", ".h")):
        return f"头文件/内联：`{path.name}` 声明或配置 AscendC/host 接口与常量。 / Header `{path.name}`."
    if rel_posix.endswith(".cpp"):
        return f"C++ 源：`{path.name}` 实现 host 或 device 逻辑。 / Source `{path.name}`."
    return f"探针资产 `{path.name}`。 / Probe file `{path.name}`."


def extract_depends(text: str, rel_posix: str) -> str:
    includes: list[str] = []
    for line in text.splitlines()[:120]:
        m = re.match(r'^\s*#include\s+["<]([^">]+)[">]', line)
        if m:
            includes.append(m.group(1))
    if includes:
        shown = ", ".join(includes[:12])
        if len(includes) > 12:
            shown += f", … (+{len(includes) - 12})"
        return f"#include: {shown}"
    if rel_posix.startswith("compute/"):
        return "compute 树内互引；host 经 main/mmad_custom 与 tiling.h 链接。"
    if rel_posix.startswith("prep/"):
        return "prep 子模块头文件 + CANN AscendC；entry 由 f203_keygen_prep_entry 聚合。"
    if rel_posix.startswith("cmake/"):
        return "CANN ascendc cmake 模块、同目录 cpu_lib/npu_lib 片段。"
    if rel_posix.startswith("scripts/"):
        return "Python3 标准库；可能 import 同目录 keygen_golden / numpy。"
    if rel_posix == "run.sh":
        return "scripts/prep、scripts/compute、cmake/*、CANN setenv、repo scripts/kernel-run-timeout.sh。"
    return "见同目录 INDEX/STATUS 或相邻 entry/host 调用链。"


def verify_for(rel_posix: str, layer: str) -> str:
    if rel_posix.startswith("scripts/") and "verify" in rel_posix:
        return "python3 调用或由 run.sh 对拍 output vs golden。"
    if rel_posix.startswith("scripts/") and ("gen_data" in rel_posix or "golden" in rel_posix):
        return "run.sh 编译前生成 input/；KEYGEN_VERIFY=1 时参与对拍。"
    if rel_posix.startswith("scripts/kat") or rel_posix.startswith("kat_liboqs"):
        return "KEYGEN_KAT=1 bash run.sh 或 kat_*.sh；对比 liboqs。"
    if rel_posix == "run.sh":
        return "bash run.sh -r cpu|sim -v Ascend910B4；bash kat_liboqs_vs_ascendc.sh；无需手动 SIM_DIRECT/HAT_*。"
    if layer in {"prep", "compute"}:
        return "经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。"
    if layer == "cmake":
        return "cmake --build 纳入 run.sh；编译失败即暴露。"
    return "随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。"


def build_block(rel_posix: str, path: Path, original: str, prefix: str) -> str:
    layer = layer_for(rel_posix)
    lines = [
        f"{MARKER}",
        f"@file {rel_posix}",
        f"@layer {layer}",
        f"@role {role_for(rel_posix, path)}",
        f"@production_io {production_io_for(rel_posix)}",
        f"@launch {launch_for(layer, rel_posix)}",
        f"@ai_core {ai_core_for(layer, rel_posix)}",
        f"@depends {extract_depends(original, rel_posix)}",
        f"@verify {verify_for(rel_posix, layer)}",
    ]
    body = "\n".join(f"{prefix} {ln}" for ln in lines)
    return body + "\n\n"


def should_process(path: Path, root: Path) -> bool:
    if path.suffix not in SOURCE_SUFFIXES:
        return False
    rel = path.relative_to(root)
    if path_excluded(rel.parts):
        return False
    return True


def inject_file(path: Path, root: Path) -> bool:
    raw = path.read_text(encoding="utf-8", errors="replace")
    if MARKER in raw:
        return False
    prefix = comment_prefix(path.suffix)
    rel_posix = path.relative_to(root).as_posix()
    block = build_block(rel_posix, path, raw, prefix)
    if raw.startswith("#!"):
        first_nl = raw.find("\n")
        if first_nl == -1:
            new_content = raw + "\n" + block
        else:
            new_content = raw[: first_nl + 1] + block + raw[first_nl + 1 :]
    else:
        new_content = block + raw
    path.write_text(new_content, encoding="utf-8")
    return True


def main() -> int:
    root = probe_root_from_script()
    updated = 0
    skipped_marker = 0
    errors: list[str] = []

    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if not should_process(path, root):
            continue
        try:
            raw = path.read_text(encoding="utf-8", errors="replace")
            if MARKER in raw:
                skipped_marker += 1
                continue
            if inject_file(path, root):
                updated += 1
                print(f"updated: {path.relative_to(root)}")
        except OSError as exc:
            errors.append(f"{path}: {exc}")

    print(f"PROBE={PROBE_MARKER_NAME}")
    print(f"root={root}")
    print(f"files_updated={updated}")
    print(f"files_already_marked={skipped_marker}")
    if errors:
        print("errors:", file=sys.stderr)
        for e in errors:
            print(e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
