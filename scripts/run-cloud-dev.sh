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
    --user 0:0 \
    --network host \
    --gpus all \
    -e DISPLAY="$DISPLAY" \
    -e HOME=/home/nick \
    -e PATH="$MSF/.venv/bin:/opt/usd/bin:/opt/venv/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v "$MSF/MADDENING":/workspace/maddening \
    -v "$MSF/MIME":/workspace/mime \
    -v "$MSF/MICROROBOTICA":/workspace/microbotica \
    -v "$MSF/.venv":"$MSF/.venv" \
    -v "$MSF/MIME":"$MSF/MIME":ro \
    -v "$MSF/MADDENING":"$MSF/MADDENING":ro \
    -v "$HOME/.sky":/home/nick/.sky \
    -v "$HOME/.kube":/home/nick/.kube:ro \
    -v "$HOME/.ssh":/home/nick/.ssh:ro \
    ghcr.io/microrobotics-simulation-framework/microrobotica:base \
    bash -c '
        # SkyPilot 0.12 requires nc + socat on PATH to talk to its k3s
        # control plane via the portforward networking mode. The :base
        # image ships neither; apt-get update first or install fails
        # to find the package.
        # SkyPilot 0.12 prereqs the :base image does NOT ship:
        #   nc + socat — Kubernetes portforward networking mode
        #   jq         — ssh-tunnel.sh exec credential plugin parses JSON
        #   autossh    — auto-reconnects the k3s control-plane SSH tunnel
        #   rsync      — sky uses it to upload workdir + file_mounts
        if ! command -v nc >/dev/null \
            || ! command -v socat >/dev/null \
            || ! command -v jq >/dev/null \
            || ! command -v autossh >/dev/null \
            || ! command -v rsync >/dev/null; then
            echo ">>> installing sky prereqs (nc, socat, jq, autossh, rsync)..."
            echo ">>> apt-get update..."
            DEBIAN_FRONTEND=noninteractive apt-get update
            echo ">>> apt-get install..."
            DEBIAN_FRONTEND=noninteractive apt-get install -y \
                --no-install-recommends \
                netcat-openbsd socat jq autossh rsync
            echo ">>> apt install done."
        fi

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
            # *.pub files can stay world-readable, but 600 is fine.
        fi

        # Keep the final shell as -i (interactive) but NOT -l, so the
        # base image profile scripts (which can drop privileges) don'\''t
        # re-fire and toss us back to ubuntu.
        exec bash -i
    '
