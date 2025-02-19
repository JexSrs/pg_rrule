FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

WORKDIR /tmp

RUN apt-get update && apt-get install -y curl ca-certificates gnupg && \
    curl https://www.postgresql.org/media/keys/ACCC4CF8.asc | gpg --dearmor | tee /etc/apt/trusted.gpg.d/apt.postgresql.org.gpg >/dev/null && \
    echo "deb http://apt.postgresql.org/pub/repos/apt jammy-pgdg main" > /etc/apt/sources.list.d/pgdg.list

RUN apt-get update && apt-get install -y \
    build-essential \
    openssl \
    cmake \
    vim \
    pkg-config \
    libgirepository1.0-dev \
    gi-docgen \
    libxml2-dev \
    gtk-doc-tools \
    postgresql-server-dev-17 \
    && rm -rf /var/lib/apt/lists/*

RUN curl -LO https://github.com/unicode-org/icu/releases/download/release-70-1/icu4c-70_1-src.tgz && \
        tar xf icu4c-70_1-src.tgz && \
        cd icu/source && \
        CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --enable-static --disable-shared --prefix=/usr/local && \
        make -j$(nproc) && \
        make install && \
        cd /tmp && \
        rm -rf icu icu4c-70_1-src.tgz

WORKDIR /app

CMD ["/bin/bash"]