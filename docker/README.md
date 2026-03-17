# MICROBOTICA Docker Images

## Images

| Image | Tag | Purpose | Rebuild frequency |
|-------|-----|---------|-------------------|
| `ghcr.io/microrobotics-simulation-framework/microrobotica` | `base` | All dependencies including OpenUSD | When dependencies change |
| `ghcr.io/microrobotics-simulation-framework/microrobotica` | `dev` | Source built and tested on top of base | Every PR / push |

## Building the base image

The base image builds OpenUSD from source (~45 min first time, cached thereafter):

```bash
docker build -f docker/Dockerfile.base \
  -t ghcr.io/microrobotics-simulation-framework/microrobotica:base .
```

## Pushing to GHCR

```bash
echo $MICROROBOTICA_DOCKER_GH_PAT | docker login ghcr.io -u USERNAME --password-stdin
docker push ghcr.io/microrobotics-simulation-framework/microrobotica:base
```

## Using for development

```bash
docker run -it --rm \
  -v $(pwd):/workspace/microbotica \
  ghcr.io/microrobotics-simulation-framework/microrobotica:base \
  bash
```

Inside the container:
```bash
cd /workspace/microbotica
cmake --preset linux-debug
cmake --build build/debug
./build/debug/tests/microbotica_tests
```

## CI usage

The GitHub Actions workflow uses the base image for fast CI:

```yaml
container:
  image: ghcr.io/microrobotics-simulation-framework/microrobotica:base
```
