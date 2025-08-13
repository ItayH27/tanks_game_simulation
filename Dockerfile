## Dockerfile
#FROM ubuntu:22.04
#
#ENV DEBIAN_FRONTEND=noninteractive
#RUN apt-get update && apt-get install -y --no-install-recommends \
#    build-essential \
#    cmake \
#    gdb \
#    ninja-build \
#    pkg-config \
# && rm -rf /var/lib/apt/lists/*
#
#ENV CMAKE_GENERATOR=Ninja

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install build tools + SSH + GDB
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    gdb \
    gdbserver \
    ninja-build \
    pkg-config \
    openssh-server \
 && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y rsync

# Configure SSH
RUN mkdir /var/run/sshd
RUN echo 'root:root' | chpasswd
RUN sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config
EXPOSE 2222

# Working directory for CLion mapping
WORKDIR /workspace

# Copy your project files
COPY . /workspace

# Default command runs SSH daemon
CMD ["/usr/sbin/sshd", "-D"]
