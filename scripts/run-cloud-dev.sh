#!/usr/bin/env bash
# Launch the MICROROBOTICA dev container with everything Cloud-mode needs:
# - sky / kubectl from the host venv (mounted at the *same path* so sky's
#   Python shebang doesn't need rewriting).
# - ~/.sky (sky_node_pools.yaml, cluster registry, askpass cache)
# - ~/.kube (k3s kubeconfig sky-ssh-up wrote during cluster bootstrap)
# - ~/.ssh  (the dedicated id_ed25519_mime key referenced by ssh_node_pools.yaml)
# - HOME set so all of the above resolve.
# Plus the existing X11 / MSF / GPU bindings the regular dev container uses.

set -e

MSF=/home/nick/MSF/msf
NAME=microbotica-cloud-dev

# Cycle any stale instance so the new mounts take effect.
docker rm -f "$NAME" 2>/dev/null || true

docker run -it --rm \
    --name "$NAME" \
    --network host \
    --gpus all \
    -e DISPLAY="$DISPLAY" \
    -e HOME=/home/nick \
    -e PATH="$MSF/.venv/bin:/opt/usd/bin:/opt/venv/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v "$MSF/MADDENING":/workspace/maddening \
    -v "$MSF/MIME":/workspace/mime \
    -v "$MSF/MICROROBOTICA":/workspace/microbotica \
    -v "$MSF/.venv":"$MSF/.venv":ro \
    -v "$MSF/MIME":"$MSF/MIME":ro \
    -v "$MSF/MADDENING":"$MSF/MADDENING":ro \
    -v "$HOME/.sky":/home/nick/.sky \
    -v "$HOME/.kube":/home/nick/.kube:ro \
    -v "$HOME/.ssh":/home/nick/.ssh:ro \
    ghcr.io/microrobotics-simulation-framework/microrobotica:base \
    bash
