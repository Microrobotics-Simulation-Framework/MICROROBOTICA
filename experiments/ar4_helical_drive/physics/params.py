# Physical parameters for the AR4 helical-UMR drive demonstrator.
#
# Pure assignments + arithmetic only — no imports, no classes.
# Executed by the MIME runner via `exec()` into a namespace.

# ─── Arm: AR4 (Annin Robotics open-source 6-DOF) ────────────────────────────
URDF_PATH = "assets/ar4.urdf"

# AR4 tool flange link name. The shipped stub URDF uses `link_6`.
END_EFFECTOR_LINK_NAME = "link_6"

# AR4 base sits at world origin; the URDF's `world` link is the static
# anchor and the `world_to_base` fixed joint introduces no offset.
BASE_POSE_WORLD = (0.0, 0.0, 0.0,  1.0, 0.0, 0.0, 0.0)

# Mount the magnet 5 cm beyond the AR4 tool flange along +x of the
# flange frame (the AR4's J6 axis). Adjust if you 3D-print a different
# magnet stage.
END_EFFECTOR_OFFSET_IN_LINK = (0.05, 0.0, 0.0,  1.0, 0.0, 0.0, 0.0)

# Per-joint viscous friction. `None` → use the URDF's `<dynamics damping>`
# (0.02 N·m·s/rad on every AR4 joint per the stub).
JOINT_FRICTION_N_M_S = None

GRAVITY_WORLD = (0.0, 0.0, -9.80665)

# ─── Motor (rotor that spins the magnet) ────────────────────────────────────
MOTOR_INERTIA_KG_M2 = 1e-5
MOTOR_KT_N_M_PER_A = 0.05
MOTOR_R_OHM = 1.0
MOTOR_L_HENRY = 1e-3
MOTOR_DAMPING_N_M_S = 1e-4
# Spin axis is the AR4 J6 axis (+x in the flange frame, which is the EE
# offset's local frame).
MOTOR_AXIS_IN_PARENT = (1.0, 0.0, 0.0)

# ─── Permanent magnet ───────────────────────────────────────────────────────
MAGNET_DIPOLE_A_M2 = 1.68e-3
MAGNET_AXIS_IN_BODY = (1.0, 0.0, 0.0)
MAGNET_RADIUS_M = 1e-3
MAGNET_LENGTH_M = 2e-3
FIELD_MODEL = "point_dipole"
EARTH_FIELD_WORLD_T = (0.0, 0.0, 0.0)

# ─── UMR + vessel ───────────────────────────────────────────────────────────
UMR_DESIGN = "FL-9"
UMR_VESSEL = '1/4"'

TIMESTEP_S = 5e-4
