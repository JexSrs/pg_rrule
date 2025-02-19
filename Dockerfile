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

WORKDIR /app

CMD ["/bin/bash"]