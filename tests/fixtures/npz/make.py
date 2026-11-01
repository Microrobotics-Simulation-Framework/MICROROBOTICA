"""The .npz files the npz reader's tests read: the same arrays stored (numpy.savez, as MIME writes outputs.npz) and
deflated (numpy.savez_compressed). Run: python tests/fixtures/npz/make.py (any NumPy)."""
from pathlib import Path

import numpy as np

here = Path(__file__).parent
arrays = {
    "final/umr/pose": np.array([0.001, -0.002, 0.003, 1.0, 0.0, 0.0, 0.0], dtype="<f4"),
    "series/x/values": np.arange(6, dtype="<f8").reshape(2, 3) * 0.5,
    "series/x/steps": np.array([33, 66], dtype="<i8"),
    "flags/f": np.array([0, 1, 1, 0], dtype="|u1"),
    "scalar": np.array(2.5, dtype="<f8"),
}
np.savez(here / "stored.npz", allow_pickle=False, **arrays)
np.savez_compressed(here / "deflated.npz", allow_pickle=False, **arrays)
