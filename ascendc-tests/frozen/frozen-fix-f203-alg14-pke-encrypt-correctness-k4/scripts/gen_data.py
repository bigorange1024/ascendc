#!/usr/bin/env python3
"""
gen_data — 生产 input only（Alg.14 Encrypt 探针）。

- ek_pke.bin：由 scripts/host_golden/gen_ek_pke.py 生成（vendored KeyGen golden，非 liboqs）
- m.bin / coins.bin：确定性随机（SEED_D）
- golden_c.bin：仅 ENCRYPT_VERIFY=1 时由 host_golden 生成（G4 前可缺省）
"""
import os
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

SCRIPT_DIR = Path(__file__).resolve().parent
CASE_DIR = SCRIPT_DIR.parent
HOST_GOLDEN = SCRIPT_DIR / "host_golden"

EK_BYTES = 1568
MSG_BYTES = 32
COINS_BYTES = 32
CT_BYTES = 1568
SEED_D_DEFAULT = 20260619


def main() -> None:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    inp = CASE_DIR / "input"
    out = CASE_DIR / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    ek_path = inp / "ek_pke.bin"
    subprocess.run(
        [sys.executable, str(HOST_GOLDEN / "gen_ek_pke.py"), str(seed_d), str(ek_path)],
        check=True,
    )

    rng = np.random.default_rng(seed_d + 991)
    m = rng.integers(0, 256, size=MSG_BYTES, dtype=np.uint8)
    coins = rng.integers(0, 256, size=COINS_BYTES, dtype=np.uint8)
    m.tofile(inp / "m.bin")
    coins.tofile(inp / "coins.bin")

    meta = struct.pack("<IIII", seed_d, EK_BYTES, MSG_BYTES, CT_BYTES)
    (inp / "meta.bin").write_bytes(meta)

    subprocess.run(
        [sys.executable, str(HOST_GOLDEN / "ntt_lut_bins.py"), str(inp)],
        check=True,
    )

    encrypt_gate = int(os.environ.get("ENCRYPT_GATE", "5"))
    if encrypt_gate < 5:
        subprocess.run(
            [sys.executable, str(HOST_GOLDEN / "decode_t_hat.py"), str(ek_path), str(inp / "t_hat.bin")],
            check=True,
        )
    else:
        print("[gen_data] ENCRYPT_GATE>=5: skip input/t_hat.bin (device ByteDecode)")

    if os.environ.get("ENCRYPT_VERIFY", "0") == "1":
        golden_py = HOST_GOLDEN / "golden_c.py"
        if not golden_py.is_file():
            print("[gen_data] ENCRYPT_VERIFY=1 but host_golden/golden_c.py missing (G4 未就绪)", file=sys.stderr)
            sys.exit(2)
        subprocess.run(
            [
                sys.executable,
                str(golden_py),
                str(ek_path),
                str(inp / "m.bin"),
                str(inp / "coins.bin"),
                str(out / "golden_c.bin"),
            ],
            check=True,
        )
        print(f"[gen_data] SEED_D={seed_d} golden_c OK")
    else:
        print(f"[gen_data] SEED_D={seed_d} ek={EK_BYTES}B m+coins + golden_c OK (ENCRYPT_VERIFY=1 默认)")


if __name__ == "__main__":
    main()
