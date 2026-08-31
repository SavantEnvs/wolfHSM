FROM ghcr.io/savantenvs/base:20260621

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
# Override the base image's ENV; wolfHSM has a misaligned uint32_t
# read/write in posix_transport_tcp.c (type-punning pattern that works on
# x86_64 but trips UBSan alignment check on every TCP connection).
# Keep UBSan enabled but disable only the alignment check.
ARG SANITIZER_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -fno-sanitize=leak,alignment"
ENV SANITIZER_FLAGS=${SANITIZER_FLAGS}
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# Pre-bake wolfSSL at a pinned commit so build.sh re-runs work offline (§6.5)
# Pinned to wolfssl master d80b4776 — compatible with wolfHSM main's user_settings.h
# (WOLFSSL_HAVE_MLKEM requires a wolfssl newer than 6c5e8417 used by mayhem-old)
USER root
RUN git clone https://github.com/wolfSSL/wolfssl /opt/toolchains/wolfssl && \
    git -C /opt/toolchains/wolfssl checkout d80b4776a97551d828a1bde0b7a8031e8e866bdf && \
    chown -R 2000:2000 /opt/toolchains/wolfssl

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN bash mayhem/build.sh
RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      org.opencontainers.image.description="wolfHSM mayhem integration"

USER mayhem
WORKDIR /mayhem
