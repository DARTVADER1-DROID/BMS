# Multi-stage build for the native C BMS API.
FROM debian:bookworm-slim AS build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        pkg-config \
        libmicrohttpd-dev \
        libcjson-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

FROM debian:bookworm-slim AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libmicrohttpd12 \
        libcjson1 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --system bms \
    && useradd --system --gid bms --home-dir /nonexistent --shell /usr/sbin/nologin bms

WORKDIR /app
COPY --from=build /src/build/bms-server /usr/local/bin/bms-server

ENV PORT=8080
EXPOSE 8080

USER bms
CMD ["/usr/local/bin/bms-server"]
