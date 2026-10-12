#Fully patched docker image for Stanford pintos

ARG TARGETPLATFORM=linux/amd64
FROM ubuntu:24.04
LABEL maintainer="Naveen Veeravalli <naven.veer@outlook.com"

# prerequisites
RUN apt-get update --fix-missing \
 && apt-get update && apt-get install -y apt-utils \
 && apt-get install -y gcc gcc-multilib make build-essential gdb git \
    wget perl patch qemu-system-x86 xorg-dev libncurses5-dev \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# Setting up the environment variables and updating the PATH
ENV PINTOSDIR=/home/pintos
ENV DSTDIR=/usr/local
ENV SRCDIR=/home/source
RUN mkdir -p $SRCDIR
RUN mkdir -p $PINTOSDIR
RUN mkdir -p $DSTDIR/bin
ENV BXSHARE=$DSTDIR/share/bochs
ENV PATH="${DSTDIR}/bin:${PATH}"

# Downloading Pintos source code
WORKDIR $SRCDIR/
RUN git clone git://pintos-os.org/pintos-anon && cd pintos-anon && git checkout f685123e5f8e7c84648b2de810ba20e85b7d1504
RUN mv pintos-anon/* $PINTOSDIR
RUN rm -rf pintos-anon
RUN rm -rf $PINTOSDIR/doc
RUN rm -rf $PINTOSDIR/specs
WORKDIR $SRCDIR/

# Installing bochs and bochs-dbg
WORKDIR $PINTOSDIR/src/misc/
RUN chmod +x ./bochs-2.6.11-build.sh
RUN ./bochs-2.6.11-build.sh /usr/local/

# Pointing pintos-gdb to the macros file
WORKDIR $PINTOSDIR/src/utils/
RUN sed -i "s|^GDBMACROS=.*$|GDBMACROS=$PINTOSDIR/src/misc/gdb-macros|" "$PINTOSDIR/src/utils/pintos-gdb"

# Make QEMU the default simulator
RUN sed -i 's/\$sim = "bochs" if !defined \$sim/\$sim = "qemu" if !defined \$sim/' "$PINTOSDIR/src/utils/pintos"

# Fixing errors with using stropts.h and its associated functions in squish-pty.c and squish-unix.c
RUN sed -i '10s@^@// @' "$PINTOSDIR/src/utils/squish-pty.c" \
 && sed -i '288,293 s@^@// @' "$PINTOSDIR/src/utils/squish-pty.c" \
 && sed -i '11s@^@// @' "$PINTOSDIR/src/utils/squish-unix.c"

# Building the binaries and installing them to the PATH
RUN make -j"$(nproc)"
RUN rm *.o
RUN cp backtrace pintos pintos-gdb pintos-mkdisk Pintos.pm pintos-set-cmdline squish-pty squish-unix $DSTDIR/bin

WORKDIR /
CMD ["bash"]
