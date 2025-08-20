# syntax=docker/dockerfile:1
FROM --platform=linux/amd64 ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    gdb \
    gdbserver \
    git \
    rsync \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
