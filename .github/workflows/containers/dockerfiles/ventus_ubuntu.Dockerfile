
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y sudo device-tree-compiler bsdmainutils ccache \
     cmake ninja-build git g++ python3 autoconf automake libtool zlib1g zlib1g-dev ruby

LABEL \
    org.opencontainers.image.title="Ubuntu Base Image of Ventus" \
    org.opencontainers.image.description="This ubuntu base image is used to build and run the software toolchain of Ventus v2.0.2." \
    org.opencontainers.image.url="https://github.com/THU-DSP-LAB" \
    org.opencontainers.image.version="2.0.2" \
    org.opencontainers.image.vendor="Ventus" \
    org.opencontainers.image.created="2024-07-05 00:00:00+00:00" \
    org.opencontainers.image.licenses="MulanPSL2"

WORKDIR /root

CMD ["/bin/bash"]
