#!/usr/bin/env bash
# Launch the MICROROBOTICA dev container with everything Cloud-mode needs:
# - sky / kubectl from the host venv (mounted at the same path so sky Python
#   shebang does not need rewriting).
# - ~/.sky (sky_node_pools.yaml, cluster registry, askpass cache)
# - ~/.kube (k3s kubeconfig sky-ssh-up wrote during cluster bootstrap)
# - ~/.ssh  (the dedicated id_ed25519_mime key referenced by ssh_node_pools.yaml)
# - HOME set so all of the above resolve.
# Plus the existing X11 / MSF bindings the regular dev container uses, and its
# rendering through Mesa (render-args.sh: the Intel GPU's /dev/dri nodes or
# llvmpipe; never an NVIDIA GPU).
#
# Hardening (added after a Cloud-mode launch attempt wedged the host):
#   - SkyPilot prereqs (nc/socat/jq/autossh/rsync) baked into a local
#     microrobotica:cloud-dev image (docker/Dockerfile.cloud-dev), so apt
#     does not run at container start in parallel with sky rsync.
#   - Resource caps: --memory=12g --cpus=6 --pids-limit=1024. A runaway
#     subprocess tree inside the container cannot starve the host.

xhost +local:docker

set -e

source "$(dirname "$(readlink -f "$0")")/render-args.sh"

MSF=/home/nick/MSF/msf
REPO=$MSF/MICROROBOTICA
NAME=microbotica-cloud-dev
IMAGE=microrobotica:cloud-dev

# Build the cloud-dev layer if it is not already present locally.
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo ">>> building $IMAGE (one-time, ~30s)..."
    docker build -f "$REPO/docker/Dockerfile.cloud-dev" -t "$IMAGE" "$REPO"
fi

# Cycle any stale instance so the new mounts take effect.
docker rm -f "$NAME" 2>/dev/null || true

docker run -it --rm \
    --name "$NAME" \
    --user 0:0 \
    --network host \
    "${RENDER_ARGS[@]}" \
    --memory=12g \
    --cpus=6 \
    --pids-limit=1024 \
    -e DISPLAY="$DISPLAY" \
    -e HOME=/home/nick \
    -e PATH="$MSF/.venv/bin:/opt/usd/bin:/opt/venv/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    -e JAX_PLATFORMS=cpu \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v "$MSF/.venv":"$MSF/.venv" \
    -v "$MSF/MIME":"$MSF/MIME" \
    -v "$MSF/MADDENING":"$MSF/MADDENING" \
    -v "$MSF/MICROROBOTICA":"$MSF/MICROROBOTICA" \
    -v "$HOME/.sky":/home/nick/.sky \
    -v "$HOME/.kube":/home/nick/.kube:ro \
    -v "$HOME/.ssh":/home/nick/.ssh:ro \
    "$IMAGE" \
    bash -c '
        # The sky API server is on the *host* (the daemon that owns
        # ~/.sky), so when MICROROBOTICA inside the container submits
        # `sky launch --workdir <path>`, the host resolves <path>
        # against the host filesystem. Mount the host paths
        # symmetrically (above) so a container-side path under
        # /home/nick/MSF/msf is identical to the host. Then symlink the
        # historical /workspace/<x> entrypoints so existing recent-file
        # caches / muscle memory still work — std::filesystem::canonical()
        # in CloudJobLauncher resolves them to the host-real path before
        # handing the workdir to sky.
        mkdir -p /workspace
        ln -sfn /home/nick/MSF/msf/MIME          /workspace/mime
        ln -sfn /home/nick/MSF/msf/MADDENING     /workspace/maddening
        ln -sfn /home/nick/MSF/msf/MICROROBOTICA /workspace/microbotica

        # The container runs as root; ssh-tunnel.sh (the kubeconfig auth
        # exec plugin) shells out to plain ssh 192.168.2.7 which looks
        # up the passwd-entry home for the running uid — /root — not
        # $HOME. AND ssh refuses any config / key file not owned by the
        # running user (or root). The bind-mounted ~/.ssh is owned by
        # the host user (uid 1000), so a symlink trips the strict
        # ownership check in ssh. Copy the files into a root-owned
        # /root/.ssh at container entry. Done once per container;
        # /root is inside the container rw layer so it does not persist.
        if [ ! -d /root/.ssh ]; then
            mkdir -p /root/.ssh
            cp -r /home/nick/.ssh/. /root/.ssh/
            chown -R root:root /root/.ssh
            chmod 700 /root/.ssh
            find /root/.ssh -type f -exec chmod 600 {} \;
        fi

        # Keep the final shell as -i (interactive) but NOT -l, so the
        # base image profile scripts (which can drop privileges) do not
        # re-fire and toss us back to ubuntu.
        exec bash -i
    '
