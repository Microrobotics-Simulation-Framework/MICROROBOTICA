"""Generic helical-UMR + URDF arm graph builder.

This module composes the de Jongh new-actuation-chain stack
(``mime.experiments.dejongh_new_chain.build_graph``) with an additional
:class:`~mime.nodes.actuation.robot_arm.RobotArmNode`. The arm's
end-effector pose is wired into the motor's ``parent_pose_world``
input, replacing the static external-input wiring used by the
arm-less de Jongh preset.

The MIME runner calls ``build_graph(params, experiment_dir)`` once at
launch. ``params`` is the dict produced by exec'ing ``params.py`` into
a fresh namespace (so all module-level names declared there are keys);
``experiment_dir`` is the absolute path to this experiment directory,
used to resolve the URDF relative to the experiment.
"""

from __future__ import annotations

import os

import jax.numpy as jnp

from mime.experiments.dejongh_new_chain import build_graph as build_dejongh_new_chain
from mime.nodes.actuation.robot_arm import RobotArmNode


def build_graph(params, experiment_dir):
    """Compose the URDF-arm + motor + permanent-magnet + UMR graph.

    Parameters
    ----------
    params : dict
        Namespace produced by ``exec()`` on ``physics/params.py``.
    experiment_dir : str
        Absolute path to the experiment directory.
    """
    # Resolve URDF path relative to the experiment directory so the
    # experiment is portable across machines.
    urdf_path = os.path.join(experiment_dir, params["URDF_PATH"])

    # ------------------------------------------------------------------
    # Build the de Jongh new-chain stack (Motor + PermanentMagnet + UMR
    # + drag) at the origin. The arm will subsequently override the
    # motor's parent pose, so `magnet_base_xyz_m` is irrelevant beyond
    # initialising the static parent pose to a finite value.
    # ------------------------------------------------------------------
    gm = build_dejongh_new_chain(
        design_name=params["UMR_DESIGN"],
        vessel_name=params["UMR_VESSEL"],
        dt=params["TIMESTEP_S"],
        magnet_base_xyz_m=(0.0, 0.0, 0.0),
        magnet_dipole_a_m2=params["MAGNET_DIPOLE_A_M2"],
        magnet_radius_m=params["MAGNET_RADIUS_M"],
        magnet_length_m=params["MAGNET_LENGTH_M"],
        field_model=params["FIELD_MODEL"],
        motor_axis_in_parent=params["MOTOR_AXIS_IN_PARENT"],
        motor_inertia_kg_m2=params["MOTOR_INERTIA_KG_M2"],
        motor_kt_n_m_per_a=params["MOTOR_KT_N_M_PER_A"],
        motor_r_ohm=params["MOTOR_R_OHM"],
        motor_l_henry=params["MOTOR_L_HENRY"],
        motor_damping_n_m_s=params["MOTOR_DAMPING_N_M_S"],
    )

    # ------------------------------------------------------------------
    # Add the RobotArmNode and wire its EE pose into the motor's
    # parent_pose_world input. The arm physically owns where the magnet
    # sits, so the static external-input wiring added by
    # `build_dejongh_new_chain` is redundant — but harmless: when an
    # internal edge supplies a boundary input, GraphManager prefers the
    # edge over external_input. We leave the external input declared
    # so legacy harnesses can still inject static poses for ablation.
    # ------------------------------------------------------------------
    arm = RobotArmNode(
        name="arm",
        timestep=params["TIMESTEP_S"],
        urdf_path=urdf_path,
        end_effector_link_name=params["END_EFFECTOR_LINK_NAME"],
        end_effector_offset_in_link=params["END_EFFECTOR_OFFSET_IN_LINK"],
        base_pose_world=params["BASE_POSE_WORLD"],
        joint_friction_n_m_s=params["JOINT_FRICTION_N_M_S"],
        gravity_world=params["GRAVITY_WORLD"],
    )
    gm.add_node(arm)
    gm.add_edge("arm", "motor", "end_effector_pose_world", "parent_pose_world")

    # Expose the arm torque command as a live external input so the
    # controller can drive it.
    n_dof = arm._num_joints
    gm.add_external_input(
        "arm", "commanded_joint_torques", shape=(n_dof,), dtype=jnp.float32,
    )

    return gm
