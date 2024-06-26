FROM centos:7.9.2009 as base

RUN yum remove gcc gcc-c++ gdb make -y \
    && yum install -y scl-utils \
    && yum install -y centos-release-scl \
    && yum install -y devtoolset-8-toolchain \
    && echo "source /opt/rh/devtoolset-8/enable" >>  /etc/profile \
    && scl enable devtoolset-8 bash \
    && yum install -y epel-release \
    && yum install -y llvm-toolset-7.0-clang.x86_64 llvm-toolset-7.0-llvm-libs.x86_64 llvm-toolset-7.0-lld.x86_64 libatomic \
    && ldconfig \
    && echo "source /opt/rh/llvm-toolset-7.0/enable" >> /etc/profile \
    && echo "/opt/rh/llvm-toolset-7.0/root/usr/lib64" > /etc/ld.so.conf.d/clang.conf \
    && ldconfig \
    && yum clean all && rm -rf /var/cache/yum /tmp/* /var/tmp/*

ENV PATH="/opt/rh/devtoolset-8/root/usr/bin:/opt/rh/llvm-toolset-7.0/root/usr/bin:$PATH"

FROM base as ventus-env

# In experience, installing git in the Dockerfile will cause the image to take a long time to build. It is recommended to install git in the container.
RUN yum remove cmake -y && yum install -y wget \
    && cd /opt/ && wget https://github.com/Kitware/CMake/releases/download/v3.22.1/cmake-3.22.1.tar.gz && tar -zxvf cmake-3.22.1.tar.gz && rm cmake-3.22.1.tar.gz \
    && yum install -y openssl openssl-devel \
    && cd /opt/cmake-3.22.1 && ./bootstrap --prefix=/usr/local/cmake && make && make install \
    && ln -s /usr/local/cmake/bin/cmake /usr/bin/cmake \
    && yum install -y python3 python3-devel ccache ninja-build which dtc words autoconf automake libtool ncurses-devel hwloc-devel hwloc hwloc-libs patch ruby git \
    && yum clean all && rm -rf /var/cache/yum /tmp/* /var/tmp/*

LABEL \
    org.opencontainers.image.title="CentOS Base Image of Ventus" \
    org.opencontainers.image.description="This centos base image is used to build and run the software toolchain of Ventus v2.0.2." \
    org.opencontainers.image.url="https://github.com/THU-DSP-LAB" \
    org.opencontainers.image.version="2.0.2" \
    org.opencontainers.image.vendor="Ventus" \
    org.opencontainers.image.created="2024-06-24 00:00:00+00:00" \
    org.opencontainers.image.licenses="MulanPSL2"

WORKDIR /root

CMD ["/bin/bash"]
