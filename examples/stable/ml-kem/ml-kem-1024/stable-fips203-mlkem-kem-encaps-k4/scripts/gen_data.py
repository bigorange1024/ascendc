#!/usr/bin/env python3
"""
gen_data.py — Alg.20 Encaps stable：ek + m + LUT + golden(c/K) + CPU golden_v。

流水线位置：`run.sh` 在 kernel 前调用；仅生成合法 input/golden（黑盒 oracle）。
FIPS：$m$ 为输入；$r$ 仅作 Host 参考派生（golden_v / 对照），不作为设备生产输入契约。
customspec：stable-fips203-mlkem-kem-encaps-k4-实现方案-customspec.*
registry：docs/specs/fips203-mlkem1024-kem-encaps-baseline-registry.md

golden 后端（2026-07-31）：ek 自举与 c/K 均交 library/shared/f203_kem_ref，**liboqs 优先，
  缺失则回落**仓内已验证参考（PKE KeyGen 借 stable KEM KeyGen 的 keygen_golden；
  PKE Encrypt 用本目录 host_golden 的 golden_c.golden_encrypt）。背景：借入实机装不了
  thirdparty/liboqs，原先硬依赖会让本算子直接跑不起来；m 仍默认 urandom，未改锁定参数。

环境覆盖：
  EK_KEM_SRC=路径   固定公钥（KAT stash）
  M_FILE / M_HEX / M_DEFAULT_HEX  控制 m（默认 os.urandom；禁止默认可全 0）
  SEED_D=int         缺 ek 时 derand 造钥
  KEEP_EK=1          复用已有 input/ek_kem.bin
  KEM_GOLDEN_BACKEND=python  强制走回落路径（自检用）；KEM_GOLDEN_CROSS=1 两条路径互校
"""
from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path

def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组嵌套）。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


import numpy as np

ROOT = Path(__file__).resolve().parent.parent
# stable-* → stable → examples → repo
REPO = _ascendc_repo_root(ROOT)
HOST_GOLDEN = ROOT / "scripts" / "host_golden"
sys.path.insert(0, str(HOST_GOLDEN))
sys.path.insert(0, str(REPO / "library" / "shared" / "fips203_host_rng"))
sys.path.insert(0, str(REPO / "library" / "shared" / "fips203_se_sample"))
sys.path.insert(0, str(REPO / "library" / "shared" / "f203_kem_ref"))
# 回落路径的 PKE KeyGen golden：借 stable KEM KeyGen 算子（自带 vendored LUT，不依赖 thirdparty/）
STABLE_KEM_KG = REPO / "examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4/scripts"

from f203_ref_common import K, N, Q, embed_message, load_lut_t_i8, stage123_transform  # noqa: E402
import golden_c as gc  # noqa: E402
import kem_ref  # noqa: E402
from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

EK_BYTES = 1568
M_BYTES = 32
SEED_D_DEFAULT = 20260619


def derand_z_from_seed(seed_d: int) -> bytes:
    """与 KEM KeyGen 设备 DerandZFromSeedD 同式（域分离串锁定）。"""
    msg = f"exp-mlkem-f203-kem-k4:SEED_Z={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def g_mh(m: bytes, ek: bytes) -> tuple[bytes, bytes]:
    """
    (K, r) ← G(m ‖ H(ek))，仅 golden / CPU 辅助。

    说明：与设备 KemEncInitHead 同式；结果写入 golden/r_ref.bin，禁止当作生产 GM 输入。
    """
    h = hashlib.sha3_256(ek).digest()
    kr = hashlib.sha3_512(m + h).digest()
    return kr[:32], kr[32:]


def _lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    """将 [*,512] LUT 折成 even/odd planar-stacked（与 Encrypt/Stage123 一致）。"""
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def _gen_luts(inp: Path) -> None:
    """写出 NTT/INTT even/odd stacked 四份 bin（与 seed/m 无关）。"""
    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    _lut_planar_stacked(lut_ntt, True).tofile(inp / "lut_ntt_even_stacked.bin")
    _lut_planar_stacked(lut_ntt, False).tofile(inp / "lut_ntt_odd_stacked.bin")
    _lut_planar_stacked(lut_intt, True).tofile(inp / "lut_intt_even_stacked.bin")
    _lut_planar_stacked(lut_intt, False).tofile(inp / "lut_intt_odd_stacked.bin")


def _make_pke_keygen(seed_d: int):
    """
    造 kem_ref 回落路径要的 PKE KeyGen golden：d(32B) → (ek_pke 1568B, dk_pke 1536B)。

    stable KEM KeyGen 的 keygen_golden 以 seed_d 为入口（内部同样 d = derand_bytes_from_seed），
    故闭包 seed_d 并断言 d 一致；仅在无 liboqs 时才真正加载，避免正常机器多载一棵脚本树。
    """

    def _pke_keygen(d: bytes) -> "tuple[bytes, bytes]":
        if d != derand_bytes_from_seed(seed_d):
            raise SystemExit("[gen_data] d 与 SEED_D 不一致，拒绝生成 golden")
        import importlib.util

        spec = importlib.util.spec_from_file_location(
            "stable_kem_keygen_golden", STABLE_KEM_KG / "keygen_golden.py"
        )
        if spec is None or spec.loader is None:
            raise SystemExit(f"[gen_data] 无法加载 {STABLE_KEM_KG / 'keygen_golden.py'}")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        kg = mod.build_full_keygen(seed_d)
        return kg["ek_pke"].tobytes(), kg["dk_pke"].tobytes()

    return _pke_keygen


def _bootstrap_ek(inp: Path, golden: Path) -> bytes:
    """
    缺省造 ek：KEM keygen_derand(d‖z from SEED_D)（liboqs 优先，缺失回落）；或 EK_KEM_SRC / KEEP_EK。

    @return ek_kem 原始字节（1568B）
    """
    ek_src = os.environ.get("EK_KEM_SRC")
    if ek_src:
        p = Path(ek_src)
        ek = p.read_bytes()
        assert len(ek) == EK_BYTES, f"ek size {len(ek)}"
        return ek
    existing = inp / "ek_kem.bin"
    if existing.is_file() and existing.stat().st_size == EK_BYTES and os.environ.get("KEEP_EK", "0") == "1":
        return existing.read_bytes()

    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    d = derand_bytes_from_seed(seed_d)
    z = derand_z_from_seed(seed_d)
    kem_seed = d + z
    ek_path = golden / "_bootstrap_ek_kem.bin"
    dk_path = golden / "_bootstrap_dk_kem.bin"
    kem_ref.kem_keygen(kem_seed, ek_path, dk_path, pke_keygen=_make_pke_keygen(seed_d))
    return ek_path.read_bytes()


def main() -> None:
    """写出 input/{ek,m,lut*,golden_v} 与 golden/{c,K,r_ref}。"""
    inp = ROOT / "input"
    golden = ROOT / "golden"
    inp.mkdir(parents=True, exist_ok=True)
    golden.mkdir(parents=True, exist_ok=True)
    (ROOT / "output").mkdir(parents=True, exist_ok=True)

    ek = _bootstrap_ek(inp, golden)
    (inp / "ek_kem.bin").write_bytes(ek)
    (inp / "ek_pke.bin").write_bytes(ek)  # 别名：兼容 Encrypt 读路径习惯

    # m 优先级：外部文件 → 显式 hex → 可选定点 M_DEFAULT_HEX → urandom。
    # 背景：曾默认可为全 0（"00"*32），削弱 Encaps 回归；与 512/768 对齐，禁止默认全 0。
    if os.environ.get("M_FILE"):
        m = Path(os.environ["M_FILE"]).read_bytes()
    elif os.environ.get("M_HEX"):
        m = bytes.fromhex(os.environ["M_HEX"])
    elif os.environ.get("M_DEFAULT_HEX"):
        m = bytes.fromhex(os.environ["M_DEFAULT_HEX"])
    else:
        m = os.urandom(M_BYTES)
    if len(m) != M_BYTES:
        raise SystemExit(f"m want {M_BYTES}B got {len(m)}B")
    if m == bytes(M_BYTES):
        raise SystemExit("m 全 0 禁止（请换 M_FILE/M_HEX/M_DEFAULT_HEX 或使用默认 urandom）")
    (inp / "m.bin").write_bytes(m)

    _k_ref, r_ref = g_mh(m, ek)
    # 仅 golden 目录：供 CPU golden_v 派生；禁止作为设备生产读入契约
    (golden / "r_ref.bin").write_bytes(r_ref)

    _gen_luts(inp)

    # CPU 辅助 golden_v（tikicpu 分段无融合 INTT 时注入 v；SIM 不读此文件）
    t_hat = gc.decode_t_hat(ek[: gc.EK_T_BYTES])
    y, _e1, e2 = gc.build_re(r_ref)  # build_re(seed=r) → (y, e1, e2)
    y_hat = stage123_transform(y, "ntt")
    tr_hat = gc.golden_tr_hat(t_hat, y_hat)
    tr_pad = np.zeros((K, N), dtype=np.int32)
    tr_pad[0] = tr_hat
    tr = stage123_transform(tr_pad, "intt")[0]
    v = embed_message(tr, m)
    v = ((v.astype(np.int64) + e2.astype(np.int64)) % Q).astype(np.int32)
    v.tofile(inp / "golden_v.bin")

    # 黑盒 oracle：encaps_derand(ek, m) → golden c/K（I/O 等价验收）
    # liboqs 优先；无 liboqs 时回落 G(m‖H(ek)) + 本目录 host_golden 的 PKE Encrypt
    src = kem_ref.kem_encaps(
        ek,
        m,
        golden / "c.bin",
        golden / "K.bin",
        pke_encrypt=gc.golden_encrypt,
        ek_path=inp / "ek_kem.bin",
    )
    print(f"[gen_data] ek ready m={m.hex()[:16]}… golden c/K via {src} encaps_derand")


if __name__ == "__main__":
    main()
