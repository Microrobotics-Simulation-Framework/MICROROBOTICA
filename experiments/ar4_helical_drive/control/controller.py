"""Minimal AR4 controller for the helical-UMR drive demonstrator.

Same shape as the generic template: zero arm torque + constant motor
spin. Replace with a closed-loop policy (e.g. visual-servoing the AR4
to track the UMR while spinning the magnet) for real use.
"""

from __future__ import annotations

import jax.numpy as jnp


# v1 sensing cheat — see template for the full TODO. Microrobot
# position is read from `state["body"]["position"]` rather than via
# sensor-role nodes.

_MOTOR_FREQ_HZ = 10.0


def reset(params):
    del params
    return {"k": 0}


def step(t, dt, observed_state, controller_state, params):
    del t, dt, params

    _umr_pos = observed_state["body"]["position"]  # noqa: F841

    arm_q = observed_state["arm"]["joint_angles"]
    arm_torque = jnp.zeros(arm_q.shape, dtype=arm_q.dtype)

    omega_des = jnp.float32(2.0 * jnp.pi * _MOTOR_FREQ_HZ)

    commanded = {
        "arm":   {"commanded_joint_torques": arm_torque},
        "motor": {"commanded_velocity": omega_des},
    }
    new_state = {"k": controller_state["k"] + 1}
    return commanded, new_state
