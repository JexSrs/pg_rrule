# Use Ubuntu:22.04 for the
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

WORKDIR /tmp

# Install posgtresql 17 (ubuntu 22.04 does not have it by default)
RUN apt-get update \
    && apt-get install -y curl ca-certificates gnupg \
    && curl https://www.postgresql.org/media/keys/ACCC4CF8.asc | gpg --dearmor | tee /etc/apt/trusted.gpg.d/apt.postgresql.org.gpg >/dev/null \
    && echo "deb http://apt.postgresql.org/pub/repos/apt jammy-pgdg main" > /etc/apt/sources.list.d/pgdg.list

# Install essential packages
RUN apt-get update && apt-get install -y \
    build-essential openssl cmake vim \
    pkg-config libgirepository1.0-dev gi-docgen gtk-doc-tools libstdc++-11-dev libgcc-11-dev \
    postgresql-server-dev-17 \
    && rm -rf /var/lib/apt/lists/*

# Download ICU from sourve and build using -fPIC params
RUN cd /tmp && \
    curl -LO https://github.com/unicode-org/icu/releases/download/release-73-2/icu4c-73_2-src.tgz && \
    tar xf icu4c-73_2-src.tgz && \
    cd icu/source && \
    CFLAGS="-fPIC" \
    CXXFLAGS="-fPIC" \
    LDFLAGS="-static-libstdc++ -static-libgcc" \
    ./configure \
        --enable-static \
        --disable-shared \
        --prefix=/usr/local \
        --with-data-packaging=static \
        --disable-renaming \
        --disable-samples \
        --disable-tests && \
    make -j$(nproc) STATICCXXFLAGS="-fPIC" STATICCFLAGS="-fPIC" && \
    make install && \
    cd /tmp && \
    rm -rf icu icu4c-73_2-src.tgz

WORKDIR /app

CMD ["/bin/bash"]