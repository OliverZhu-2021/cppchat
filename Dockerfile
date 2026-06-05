# ── Stage 1: Builder ──────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# 1. System build tools + all apt-available library dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ca-certificates \
    pkg-config \
    libssl-dev \
    libcurl4-openssl-dev \
    libmysqlcppconn-dev \
    default-libmysqlclient-dev \
    libopencv-dev \
    librabbitmq-dev \
    nlohmann-json3-dev \
    libboost-chrono-dev \
    libboost-system-dev \
    libboost-thread-dev \
  && rm -rf /var/lib/apt/lists/*

# 2. Muduo network library — built from pre-downloaded tarball
COPY deps/muduo-v2.0.2.tar.gz /tmp/muduo.tar.gz
RUN tar -xzf /tmp/muduo.tar.gz -C /tmp \
  && mv /tmp/muduo-2.0.2 /tmp/muduo \
  && cmake -S /tmp/muduo -B /tmp/muduo/build \
       -DCMAKE_BUILD_TYPE=Release \
       -DMUDUO_BUILD_EXAMPLES=OFF \
       -DMUDUO_BUILD_TESTS=OFF \
  && cmake --build /tmp/muduo/build --parallel $(nproc) \
  && cmake --install /tmp/muduo/build \
  && rm -rf /tmp/muduo /tmp/muduo.tar.gz

# 3. ONNX Runtime 1.20.0 — installed from pre-downloaded tarball
COPY deps/onnxruntime-linux-x64-1.20.0.tgz /tmp/ort.tgz
RUN tar -xzf /tmp/ort.tgz -C /tmp \
  && cp -r /tmp/onnxruntime-linux-x64-1.20.0/include/. /usr/local/include/ \
  && cp -r /tmp/onnxruntime-linux-x64-1.20.0/lib/.     /usr/local/lib/ \
  && ldconfig \
  && rm -rf /tmp/ort.tgz /tmp/onnxruntime-linux-x64-1.20.0

# 4. SimpleAmqpClient — built from pre-downloaded tarball
COPY deps/SimpleAmqpClient-v2.5.1.tar.gz /tmp/SimpleAmqpClient.tar.gz
RUN tar -xzf /tmp/SimpleAmqpClient.tar.gz -C /tmp \
  && mv /tmp/SimpleAmqpClient-2.5.1 /tmp/SimpleAmqpClient \
  && cmake -S /tmp/SimpleAmqpClient -B /tmp/SimpleAmqpClient/build \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_INSTALL_PREFIX=/usr/local \
  && cmake --build /tmp/SimpleAmqpClient/build --parallel $(nproc) \
  && cmake --install /tmp/SimpleAmqpClient/build \
  && ldconfig \
  && rm -rf /tmp/SimpleAmqpClient /tmp/SimpleAmqpClient.tar.gz

# 5. Build the application
WORKDIR /src
COPY . .

RUN cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
  && cmake --build build --parallel 1

# ── Stage 2: Runtime ──────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    libcurl4 \
    libmysqlcppconn-dev \
    libmysqlclient21 \
    libopencv-core4.5d \
    libopencv-imgproc4.5d \
    libopencv-highgui4.5d \
    libopencv-imgcodecs4.5d \
    libopencv-videoio4.5d \
    libopencv-dnn4.5d \
    librabbitmq4 \
    libboost-chrono1.74.0 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
  && rm -rf /var/lib/apt/lists/*

# Copy manually-installed shared libraries from builder
COPY --from=builder /usr/local/lib/libonnxruntime*.so*      /usr/local/lib/
COPY --from=builder /usr/local/lib/libSimpleAmqpClient*.so* /usr/local/lib/

RUN ldconfig

# Copy MobileNetV2 ONNX model and ImageNet labels from pre-downloaded files
RUN mkdir -p /root/models/mobilenetv2
COPY deps/mobilenetv2-7.onnx    /root/models/mobilenetv2/mobilenetv2-7.onnx
COPY deps/imagenet_classes.txt  /root/imagenet_classes.txt

# Binary CWD is /app/build so that ../AIApps/ChatServer/resource/config.json resolves correctly
WORKDIR /app
COPY --from=builder /src/build/http_server          ./build/http_server
COPY --from=builder /src/AIApps/ChatServer/resource ./AIApps/ChatServer/resource

WORKDIR /app/build
EXPOSE 8116
CMD ["./http_server", "-p", "8116"]
