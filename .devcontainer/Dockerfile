
# Use the official Microsoft Alpine devcontainer image as a base
FROM mcr.microsoft.com/devcontainers/base:alpine


# Install GCC with C23 support and essential build tools
RUN apk update \
    && apk add --no-cache \
        gcc \
        g++ \
        clang \
        make \
        cmake \
        gdb \
        git \
        sudo


# Set GCC as the default compiler
ENV CC=gcc
ENV CXX=g++


# Set the default shell to sh
SHELL ["/bin/sh", "-c"]


# Set up a non-root user (optional, recommended for devcontainers)
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000
RUN addgroup -g $USER_GID $USERNAME || true \
    && adduser -D -u $USER_UID -G $USERNAME $USERNAME || true \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

USER $USERNAME
WORKDIR /workspaces/parallel-prog
