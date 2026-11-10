FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake pkg-config libmicrohttpd-dev libcjson-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY backend-c ./backend-c
RUN cmake -S backend-c -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build --parallel && ctest --test-dir build --output-on-failure
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends libmicrohttpd12 libcjson1 ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /src/build/bms-server /app/bms-server
ENV PORT=8080
EXPOSE 8080
USER 10001
ENTRYPOINT ["/app/bms-server"]
