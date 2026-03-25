# Physical parameters for UMR confinement experiment.
# Pure assignments + arithmetic only — no imports, no classes.
# Executed by MIME runner via exec() into a namespace.

CHANNEL_RADIUS = 0.0047       # m
ROBOT_LENGTH = 0.001          # m
ROBOT_RADIUS = 0.0003         # m
CONFINEMENT_RATIO = ROBOT_RADIUS / CHANNEL_RADIUS  # ~0.064

FIELD_FREQUENCY_HZ = 60.0     # Hz
FIELD_STRENGTH_MT = 10.0       # mT

LATTICE_RESOLUTION = 64        # LBM grid points per diameter
DT_PHYS = 3.1e-4              # s — physics timestep
