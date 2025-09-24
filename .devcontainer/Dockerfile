# Use the official Ubuntu 24.04 image as a base
FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

# Install GCC with C23 support and essential build tools
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        gcc \
        g++ \
        make \
        cmake \
        gdb \
        git \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Set GCC as the default compiler
ENV CC=gcc
ENV CXX=g++

# Set the default shell to bash
SHELL ["/bin/bash", "-c"]

# Set up a non-root user (optional, recommended for devcontainers)
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000
RUN if ! getent group $USERNAME >/dev/null; then groupadd --gid $USER_GID $USERNAME; fi \
    && if ! id -u $USERNAME >/dev/null 2>&1; then useradd --uid $USER_UID --gid $USER_GID -m $USERNAME; fi \
    && apt-get update && apt-get install -y sudo \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

USER $USERNAME
WORKDIR /workspaces/parallel-prog
