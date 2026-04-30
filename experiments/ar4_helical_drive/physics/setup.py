"""AR4 + helical-UMR graph builder.

Identical wiring to the generic template
(`_template_helical_drive/physics/setup.py`); only the URDF and EE link
name differ (those come from `params.py`). See the template's setup.py
for prose.
"""

from __future__ import annotations

import os

import jax.numpy as jnp

from mime.experiments.dejongh_new_chain import build_graph as build_dejongh_new_chain
from mime.nodes.actuation.robot_arm import RobotArmNode


def build_graph(params, experiment_dir):
    """Compose the AR4 + motor + permanent-magnet + UMR graph."""
    urdf_path = os.path.join(experiment_dir, params["URDF_PATH"])

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
        use_coupling_group=params.get("USE_COUPLING_GROUP", True),
    )

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

    n_dof = arm._num_joints
    gm.add_external_input(
        "arm", "commanded_joint_torques", shape=(n_dof,), dtype=jnp.float32,
    )

    return gm
