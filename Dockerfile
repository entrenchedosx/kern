# Minimal image to build and run Kern (headless; no Raylib).
# docker build -t kern:dev .
# docker run --rm kern:dev

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DKERN_BUILD_GAME=OFF \
      -DKERN_BUILD_DOC_FRAMEWORK_DEMO=OFF \
    && cmake --build build --parallel --target kern kernc kern-scan

CMD ["./build/kern", "--version"]
