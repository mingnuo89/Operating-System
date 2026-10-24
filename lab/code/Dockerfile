FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        binutils-riscv64-unknown-elf \
        build-essential \
        ca-certificates \
        device-tree-compiler \
        file \
        gcc-riscv64-unknown-elf \
        gdb-multiarch \
        git \
        opensbi \
        python3 \
        qemu-system-data \
        qemu-system-misc \
    && usermod --login student --home /home/student --move-home --shell /bin/bash ubuntu \
    && groupmod --new-name student ubuntu \
    && usermod --groups student student \
    && mkdir --parents /workspace \
    && chown student:student /workspace \
    && rm --recursive --force /var/lib/apt/lists/*

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        bc \
        curl \
        gdb \
        libfdt-dev \
        libglib2.0-dev \
        libpixman-1-dev \
        pkg-config \
        xz-utils \
        zlib1g-dev \
        cscope \
        universal-ctags \
    && ln --symbolic /usr/bin/gdb-multiarch /usr/local/bin/riscv64-unknown-elf-gdb \
    && rm --recursive --force /var/lib/apt/lists/*

COPY --chmod=0755 docker/qemu-system-riscv64 /usr/local/bin/qemu-system-riscv64

ENV RISCV=/usr

WORKDIR /workspace

USER student

CMD ["sleep", "infinity"]
