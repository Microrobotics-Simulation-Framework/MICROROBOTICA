# Physical parameters for the generic helical-UMR drive template.
#
# Pure assignments + arithmetic only — no imports, no classes. Executed
# by the MIME runner via `exec()` into a namespace, so every name
# defined here becomes a key in the `params` dict that `setup.py` and
# `controller.py` receive.

# ─── Arm (URDF-driven) ──────────────────────────────────────────────────────
# Path to URDF — resolved relative to the experiment directory by setup.py.
URDF_PATH = "assets/robot.urdf"

# End-effector link to which the motor housing is attached. The template
# ships a 3-link planar arm whose distal link is named "link_3"; if you
# swap the URDF, update this name to match.
END_EFFECTOR_LINK_NAME = "link_3"

# World-frame pose of the URDF base link [x, y, z, qw, qx, qy, qz].
BASE_POSE_WORLD = (0.0, 0.0, 0.0,  1.0, 0.0, 0.0, 0.0)

# Constant transform from the EE link's frame to the motor housing.
# Default places the magnet ~5 cm beyond the link's origin along +z.
END_EFFECTOR_OFFSET_IN_LINK = (0.0, 0.0, 0.05,  1.0, 0.0, 0.0, 0.0)

# Per-joint viscous friction. `None` → use the URDF's `<dynamics damping>`.
JOINT_FRICTION_N_M_S = None

# World-frame gravity vector [m/s^2].
GRAVITY_WORLD = (0.0, 0.0, -9.80665)

# ─── Motor (rotor that spins the magnet) ────────────────────────────────────
MOTOR_INERTIA_KG_M2 = 1e-5
MOTOR_KT_N_M_PER_A = 0.05
MOTOR_R_OHM = 1.0
MOTOR_L_HENRY = 1e-3
MOTOR_DAMPING_N_M_S = 1e-4
# Rotor spin axis expressed in the EE-attached parent frame.
MOTOR_AXIS_IN_PARENT = (0.0, 0.0, 1.0)

# ─── Permanent magnet on the rotor ──────────────────────────────────────────
# Two 1 mm³ N45 magnets — matches the de Jongh benchmark calibration.
MAGNET_DIPOLE_A_M2 = 1.68e-3
MAGNET_AXIS_IN_BODY = (1.0, 0.0, 0.0)
MAGNET_RADIUS_M = 1e-3
MAGNET_LENGTH_M = 2e-3
FIELD_MODEL = "point_dipole"
EARTH_FIELD_WORLD_T = (0.0, 0.0, 0.0)

# ─── UMR + vessel ───────────────────────────────────────────────────────────
# FL-9 helix in 1/4" tubing, matching the de Jongh confined-swimming preset.
UMR_DESIGN = "FL-9"
UMR_VESSEL = '1/4"'

# Physics integration timestep [s].
TIMESTEP_S = 5e-4
