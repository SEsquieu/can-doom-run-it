FROM ubuntu:24.04@sha256:2198e4c65076e9cb0b3d695219df667c0e3b0072da64ca3c2648aca17dcecaf5

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --no-install-recommends -y \
    build-essential ca-certificates git python3 python3-venv pkg-config \
    libsdl2-dev libvpx-dev libbz2-dev libgl-dev libx11-dev libxcursor-dev \
    libasound2-dev libxcb1-dev libopenal1 libxtst6 xvfb xauth freedoom \
    && rm -rf /var/lib/apt/lists/*
RUN python3 -m venv /opt/build-tools \
    && /opt/build-tools/bin/pip install --no-cache-dir cmake==3.31.10 ninja==1.13.0
ENV PATH="/opt/build-tools/bin:${PATH}"

# Ubuntu's base image supplies uid 1000 as the ubuntu user.
USER ubuntu
WORKDIR /workspaces/can-doom-run-it
