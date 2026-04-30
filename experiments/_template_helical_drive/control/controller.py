"""Minimal controller for the helical-UMR drive template.

This controller holds the arm at zero torque (so it falls to a static
equilibrium under gravity + joint damping) and spins the motor at a
constant 10 Hz. It exists primarily to demonstrate the input/output
plumbing — replace it with a closed-loop policy for real experiments.

API:
    reset(params)                     -> controller_state
    step(t, dt, observed_state,
         controller_state, params)    -> (commanded_inputs, controller_state)

`observed_state` is a dict keyed by node name, with each value being
the observable subset of that node's state. The MIME runner builds it
from each node's `observable_fields()` list.
"""

from __future__ import annotations

import jax.numpy as jnp


# ---------------------------------------------------------------------------
# v1 sensing cheat
# ---------------------------------------------------------------------------
# TODO(sensing): the controller currently reads the microrobot position
# straight from `state["body"]["position"]`. This is a dev-loop shortcut
# that will be replaced by sensor-role nodes (camera, magnetometer,
# ultrasound) wired through the MIME sensing pipeline. See the sensing
# plan referenced in the architecture doc.
# ---------------------------------------------------------------------------


# Constant motor spin frequency [Hz].
_MOTOR_FREQ_HZ = 10.0


def reset(params):
    """Return initial controller state.

    Parameters
    ----------
    params : dict
        Namespace produced by exec'ing ``physics/params.py``.
    """
    del params  # unused
    return {"k": 0}


def step(t, dt, observed_state, controller_state, params):
    """Compute commanded inputs for the next physics step."""
    del t, dt, params  # unused in this stub

    # v1 cheat: read the microrobot position directly from body state.
    # Kept around because closed-loop variants of this template will
    # use it; not consumed by the constant-speed controller below.
    _umr_pos = observed_state["body"]["position"]  # noqa: F841

    # Arm: hold zero commanded torque. Joint damping in the URDF
    # provides the only restoring action, so the arm settles into a
    # gravity-loaded static pose.
    arm_q = observed_state["arm"]["joint_angles"]
    arm_torque = jnp.zeros(arm_q.shape, dtype=arm_q.dtype)

    # Motor: spin at the constant target frequency.
    omega_des = jnp.float32(2.0 * jnp.pi * _MOTOR_FREQ_HZ)

    commanded = {
        "arm":   {"commanded_joint_torques": arm_torque},
        "motor": {"commanded_velocity": omega_des},
    }
    new_state = {"k": controller_state["k"] + 1}
    return commanded, new_state
