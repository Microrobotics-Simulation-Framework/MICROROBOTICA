# AR4 STL meshes

The `scene/world.usda` file references the following STL meshes (one
per AR4 link). They are NOT included in this repository — they ship
with the upstream Annin Robotics AR4 distribution under a
non-redistributable license. To make this experiment renderable, fetch
them yourself:

```
git clone https://github.com/Chris-Annin/AR4 /tmp/ar4
cp /tmp/ar4/ar4_description/meshes/visual/*.stl \
   MICROROBOTICA/experiments/ar4_helical_drive/assets/meshes/
```

Expected files in this directory:

- `base_link.stl`
- `link_1.stl`
- `link_2.stl`
- `link_3.stl`
- `link_4.stl`
- `link_5.stl`
- `link_6.stl`

The mesh filenames in upstream's repo may differ (e.g. `link1.STL` vs
`link_1.stl`); rename or symlink as needed to match the references in
`scene/world.usda`. The exact upstream naming convention drifts across
AR4 firmware releases — pin to a specific commit if you depend on
this for a reproducible run.

Until the meshes are present, MIME and the MICROBOTICA viewer will
treat the link prims as empty Xforms — physics still runs (the URDF
provides all dynamics inputs), only visualisation is degraded.
