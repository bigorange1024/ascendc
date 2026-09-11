#!/usr/bin/python3
# coding=utf-8

import numpy as np

M = np.array([])


def _sympy_ntt():
    try:
        from sympy.discrete.transforms import ntt
    except Exception as exc:
        raise RuntimeError(
            "SymPy is required for NTT_REF=sympy. Install it with: "
            "python3 -m pip install sympy"
        ) from exc
    return ntt


def standard_ntt(coeffs, n, q):
    ntt = _sympy_ntt()
    data = [int(c) % int(q) for c in coeffs]
    if len(data) != n:
        raise ValueError(f"expected {n} coefficients, got {len(data)}")
    return np.array(ntt(data, prime=int(q)), dtype=np.int32)


def standard_ntt_matrix(n, q):
    ntt = _sympy_ntt()
    rows = []
    for row in range(n):
        e = [0] * n
        e[row] = 1
        rows.append(ntt(e, prime=int(q)))
    return np.array(rows, dtype=np.int32)


def gen_standard_data(n, q, bench):
    global M

    print("=" * 60)
    print(f"SymPy standard cyclic NTT: N={n}, Q={q}")
    print("=" * 60)

    M = standard_ntt_matrix(n, q)
    inputs = []
    goldens = []
    for _ in range(bench):
        x = np.random.randint(0, q, size=n, dtype=int)
        z = standard_ntt(x, n, q)
        inputs.append(x.astype(np.int32))
        goldens.append(z.astype(np.int32))

    return (np.concatenate(inputs).astype(np.int32),
            np.concatenate(goldens).astype(np.int32))


def bit_reverse(x, logn):
    r = 0
    for _ in range(logn):
        r = (r << 1) | (x & 1)
        x >>= 1
    return r


def reorder_bitrev(a):
    n = len(a)
    logn = n.bit_length() - 1
    b = [0] * n
    for i in range(n):
        b[bit_reverse(i, logn)] = a[i]
    return b


def compare_with_standard(input_x, golden, n, q, max_cases=4):
    cases = min(int(max_cases), len(input_x) // n)
    if cases <= 0:
        return True

    natural_ok = True
    bitrev_ok = True
    first_mismatch = None
    for case in range(cases):
        x = input_x[case * n:(case + 1) * n]
        y = golden[case * n:(case + 1) * n]
        std = standard_ntt(x, n, q)
        std_bitrev = np.array(reorder_bitrev(std), dtype=np.int32)

        if not np.array_equal(y, std):
            natural_ok = False
            if first_mismatch is None:
                bad = np.flatnonzero(y != std)
                if len(bad):
                    j = int(bad[0])
                    first_mismatch = (case, j, int(y[j]), int(std[j]))
        if not np.array_equal(y, std_bitrev):
            bitrev_ok = False

    print("[standard NTT compare] checked cases:", cases)
    print("[standard NTT compare] natural order match:", natural_ok)
    print("[standard NTT compare] bit-reversed order match:", bitrev_ok)
    if first_mismatch is not None:
        case, j, got, expected = first_mismatch
        print(
            "[standard NTT compare] first natural mismatch: "
            f"case={case}, index={j}, current={got}, standard={expected}"
        )
    return natural_ok or bitrev_ok
