# MICROBOTICA CI/Development Image
#
# Layers the MICROBOTICA source and build on top of the base image.
# Used by CI workflows and for contributor development.
#
# Usage:
#   docker build -f docker/Dockerfile -t ghcr.io/microrobotics-simulation-framework/microrobotica:dev .
#
# Requires the base image to be built first:
#   docker build -f docker/Dockerfile.base -t ghcr.io/microrobotics-simulation-framework/microrobotica:base .

FROM ghcr.io/microrobotics-simulation-framework/microrobotica:base AS build

# Copy source
COPY . /workspace/microbotica
WORKDIR /workspace/microbotica

# Configure and build (debug)
RUN cmake --preset linux-debug \
    && cmake --build build/debug

# Build and run tests
RUN cmake --build build/debug --target microbotica_tests \
    && cd build/debug && ctest --output-on-failure

# Run compliance scripts
RUN python scripts/check_anomalies.py \
    && python scripts/check_citations.py \
    && python scripts/harvest_component_meta.py > /dev/null

# ASan build
RUN cmake --preset linux-asan \
    && cmake --build build/asan --target microbotica_tests \
    && LSAN_OPTIONS=suppressions=lsan.suppressions ./build/asan/tests/microbotica_tests

LABEL org.opencontainers.image.source="https://github.com/Microrobotics-Simulation-Framework/MICROROBOTICA"
LABEL org.opencontainers.image.description="MICROBOTICA CI image — built and tested"
