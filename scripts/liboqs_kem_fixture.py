#!/usr/bin/env python3
"""
liboqs_kem_fixture.py — 生成 liboqs KEM 全链黑盒向量（可切换 ML-KEM-512/768/1024）。

覆盖 KeyGen / Encaps / Decaps + 隐式拒绝路径，供
`liboqs_kem_vs_ascendc.sh` / `stable_kem_liboqs_roundtrip.sh` 与 AscendC 对拍。

参数组：
  --param 512|768|1024   或环境变量 MLKEM_PARAM（默认 1024，兼容旧调用）

两种种子模式（互斥）：

1) **--random**（推荐办公室 round-trip）：
   - kem_seed = os.urandom(64) = d(32)‖z(32)
   - m        = os.urandom(32)
   - AscendC：KeyGen `KEM_KG_EXT_SEED=1`；Encaps `M_FILE=m.bin`

2) **--seed-d / SEED_D**（定点 derand）：
   - 1024：沿用历史域分离串（k4），保证旧 KAT 可复现
   - 512/768：用 `exp-mlkem-f203-kem-k{k}:…` 新串（勿与 1024 混用）

产出（--out-dir）：
- kem_seed.bin / m.bin / d.bin / z.bin
- ek_kem.bin / dk_kem.bin / c.bin / K.bin
- K_decaps.bin / c_bad.bin / K_reject.bin
- param.txt（写入 tag，便于下游核对）
"""
from __future__ import annotations

import argparse
import hashlib
import os
import struct
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
FIPS203_SE = REPO_ROOT / "library/shared/fips203_se_sample"
REF_BIN = REPO_ROOT / "scripts/liboqs_kem_ref"
BUILD_REF = REPO_ROOT / "scripts/build_liboqs_kem_ref.sh"

sys.path.insert(0, str(REPO_ROOT / "scripts"))
from mlkem_param import resolve_param  # noqa: E402

sys.path.insert(0, str(FIPS203_SE))
from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

SEED_D_DEFAULT = 20260619


def derand_z_from_seed(seed_d: int, k: int) -> bytes:
    """KeyGen 隐式拒绝秘密 z 的 host 派生。"""
    if k == 4:
        msg = f"exp-mlkem-f203-kem-k4:SEED_Z={seed_d}".encode()
    else:
        msg = f"exp-mlkem-f203-kem-k{k}:SEED_Z={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def derand_m_from_seed(seed_d: int, k: int) -> bytes:
    """Encaps 消息种子 m 的 host 派生（定点路径；--random 不用）。"""
    if k == 4:
        msg = f"exp-mlkem-f203-kem-encaps-k4:SEED_M={seed_d}".encode()
    else:
        msg = f"exp-mlkem-f203-kem-encaps-k{k}:SEED_M={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def _ensure_ref() -> Path:
    if REF_BIN.is_file():
        return REF_BIN
    subprocess.check_call(["bash", str(BUILD_REF)])
    return REF_BIN


def _parse_hex(name: str, hex_str: str, n: int) -> bytes:
    raw = bytes.fromhex(hex_str)
    if len(raw) != n:
        raise SystemExit(f"[liboqs_kem_fixture] {name} want {n}B got {len(raw)}B")
    return raw


def _run_ref(ref: Path, param: str, args: list[str]) -> None:
    env = os.environ.copy()
    env["MLKEM_PARAM"] = param
    subprocess.check_call([str(ref), "--param", param, *args], env=env)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--param",
        default=os.environ.get("MLKEM_PARAM", "1024"),
        help="ML-KEM 参数组：512|768|1024（默认 1024）",
    )
    ap.add_argument(
        "--random",
        action="store_true",
        help="os.urandom 生成 kem_seed(64)=d‖z 与 m(32)，再调 liboqs derand",
    )
    ap.add_argument(
        "--kem-seed-hex",
        default=os.environ.get("KEM_SEED_HEX", ""),
        help="可选：固定 64B kem_seed 的 hex",
    )
    ap.add_argument(
        "--m-hex",
        default=os.environ.get("M_HEX", ""),
        help="可选：固定 32B m 的 hex",
    )
    ap.add_argument("--seed-d", type=int, default=int(os.environ.get("SEED_D", SEED_D_DEFAULT)))
    ap.add_argument("--out-dir", type=Path, required=True)
    args = ap.parse_args()

    p = resolve_param(args.param)
    tag = p["tag"]
    k = int(p["k"])
    ek_n, dk_n, ct_n, ss_n = int(p["ek"]), int(p["dk"]), int(p["ct"]), int(p["ss"])
    kem_seed_n, m_n = int(p["kem_seed"]), int(p["encaps_seed"])

    out = args.out_dir
    out.mkdir(parents=True, exist_ok=True)
    (out / "param.txt").write_text(f"{tag}\n", encoding="utf-8")

    if args.random or args.kem_seed_hex or args.m_hex:
        if args.kem_seed_hex:
            kem_seed = _parse_hex("kem_seed", args.kem_seed_hex, kem_seed_n)
        elif args.random:
            kem_seed = os.urandom(kem_seed_n)
        else:
            raise SystemExit(
                "[liboqs_kem_fixture] --m-hex 单独使用时须同时给 --kem-seed-hex 或 --random"
            )

        if args.m_hex:
            m = _parse_hex("m", args.m_hex, m_n)
        elif args.random:
            m = os.urandom(m_n)
        else:
            raise SystemExit(
                "[liboqs_kem_fixture] --kem-seed-hex 单独使用时须同时给 --m-hex 或 --random"
            )

        d, z = kem_seed[:32], kem_seed[32:]
        mode = "random/explicit"
        (out / "seed_d.bin").write_bytes(b"")
    else:
        d = derand_bytes_from_seed(args.seed_d)
        z = derand_z_from_seed(args.seed_d, k)
        m = derand_m_from_seed(args.seed_d, k)
        kem_seed = d + z
        mode = f"SEED_D={args.seed_d}"
        (out / "seed_d.bin").write_bytes(struct.pack("<I", args.seed_d))

    (out / "d.bin").write_bytes(d)
    (out / "z.bin").write_bytes(z)
    (out / "m.bin").write_bytes(m)
    (out / "kem_seed.bin").write_bytes(kem_seed)

    ref = _ensure_ref()
    ek_path = out / "ek_kem.bin"
    dk_path = out / "dk_kem.bin"
    _run_ref(ref, tag, ["keygen", str(ek_path), str(dk_path), kem_seed.hex()])

    c_path = out / "c.bin"
    k_path = out / "K.bin"
    _run_ref(ref, tag, ["encaps", str(ek_path), str(c_path), str(k_path), m.hex()])

    kdec_path = out / "K_decaps.bin"
    _run_ref(ref, tag, ["decaps", str(dk_path), str(c_path), str(kdec_path)])

    c_bytes = bytearray(c_path.read_bytes())
    c_bytes[0] ^= 0x01
    c_bad_path = out / "c_bad.bin"
    c_bad_path.write_bytes(bytes(c_bytes))
    krej_path = out / "K_reject.bin"
    _run_ref(ref, tag, ["decaps", str(dk_path), str(c_bad_path), str(krej_path)])

    # 长度门禁
    for path, want, label in (
        (ek_path, ek_n, "ek_kem"),
        (dk_path, dk_n, "dk_kem"),
        (c_path, ct_n, "c"),
        (k_path, ss_n, "K"),
        (kdec_path, ss_n, "K_decaps"),
        (krej_path, ss_n, "K_reject"),
    ):
        got = path.stat().st_size
        if got != want:
            raise SystemExit(f"[liboqs_kem_fixture] {label} size got={got} want={want} (param={tag})")

    k_enc = k_path.read_bytes()
    k_dec = kdec_path.read_bytes()
    k_rej = krej_path.read_bytes()
    j_zc = hashlib.shake_256(z + bytes(c_bytes)).digest(32)
    assert k_enc == k_dec, "[liboqs_kem_fixture] BUG: Encaps K != Decaps K'"
    assert k_rej == j_zc, "[liboqs_kem_fixture] BUG: reject K != J(z||c_bad)"
    assert k_rej != k_enc, "[liboqs_kem_fixture] BUG: 拒绝秘密与合法秘密相同"

    print(f"[liboqs_kem_fixture] param={tag} ({p['oqs_alg']}) mode={mode} -> {out}")
    print(f"[liboqs_kem_fixture] sizes ek={ek_n} dk={dk_n} c={ct_n}")
    print(f"[liboqs_kem_fixture] kem_seed={kem_seed.hex()[:16]}… m={m.hex()[:16]}…")
    print("[liboqs_kem_fixture] keygen/encaps/decaps/reject vectors OK (self-check passed)")


if __name__ == "__main__":
    main()
