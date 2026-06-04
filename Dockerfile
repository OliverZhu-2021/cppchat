# ── Stage 1: Builder ──────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# 1. System build tools + all apt-available library dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    wget \
    ca-certificates \
    pkg-config \
    libssl-dev \
    libcurl4-openssl-dev \
    libmysqlcppconn-dev \
    default-libmysqlclient-dev \
    libopencv-dev \
    librabbitmq-dev \
    nlohmann-json3-dev \
  && rm -rf /var/lib/apt/lists/*

# 2. Muduo network library (not in apt)
RUN git clone --depth 1 --branch v2.0.2 \
        https://github.com/chenshuo/muduo.git /tmp/muduo \
  && cmake -S /tmp/muduo -B /tmp/muduo/build \
       -DCMAKE_BUILD_TYPE=Release \
       -DMUDUO_BUILD_EXAMPLES=OFF \
       -DMUDUO_BUILD_TESTS=OFF \
  && cmake --build /tmp/muduo/build --parallel $(nproc) \
  && cmake --install /tmp/muduo/build \
  && rm -rf /tmp/muduo

# 3. ONNX Runtime 1.20.0 binary release (not in apt)
RUN wget -q \
      https://github.com/microsoft/onnxruntime/releases/download/v1.20.0/onnxruntime-linux-x64-1.20.0.tgz \
      -O /tmp/ort.tgz \
  && tar -xzf /tmp/ort.tgz -C /tmp \
  && cp -r /tmp/onnxruntime-linux-x64-1.20.0/include/. /usr/local/include/ \
  && cp -r /tmp/onnxruntime-linux-x64-1.20.0/lib/.     /usr/local/lib/ \
  && ldconfig \
  && rm -rf /tmp/ort.tgz /tmp/onnxruntime-linux-x64-1.20.0

# 4. SimpleAmqpClient (not in apt; depends on librabbitmq-dev)
RUN git clone --depth 1 --branch v2.5.1 \
        https://github.com/alanxz/SimpleAmqpClient.git /tmp/SimpleAmqpClient \
  && cmake -S /tmp/SimpleAmqpClient -B /tmp/SimpleAmqpClient/build \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_INSTALL_PREFIX=/usr/local \
  && cmake --build /tmp/SimpleAmqpClient/build --parallel $(nproc) \
  && cmake --install /tmp/SimpleAmqpClient/build \
  && ldconfig \
  && rm -rf /tmp/SimpleAmqpClient

# 5. Build the application
WORKDIR /src
COPY . .

RUN cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
  && cmake --build build --parallel $(nproc)

# ── Stage 2: Runtime ──────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    libcurl4 \
    libmysqlcppconn9 \
    libmysqlclient21 \
    libopencv-core4.5d \
    libopencv-imgproc4.5d \
    libopencv-highgui4.5d \
    libopencv-imgcodecs4.5d \
    libopencv-videoio4.5d \
    libopencv-dnn4.5d \
    librabbitmq4 \
    wget \
    ca-certificates \
  && rm -rf /var/lib/apt/lists/*

# Copy manually-installed shared libraries from builder
COPY --from=builder /usr/local/lib/libonnxruntime*.so*      /usr/local/lib/
COPY --from=builder /usr/local/lib/libSimpleAmqpClient*.so* /usr/local/lib/

RUN ldconfig

# Download MobileNetV2 ONNX model and ImageNet labels
# Override at runtime by mounting a volume and setting ONNX_MODEL_PATH / ONNX_LABEL_PATH
RUN mkdir -p /root/models/mobilenetv2 \
  && wget -q \
       https://github.com/onnx/models/raw/main/validated/vision/classification/mobilenet/model/mobilenetv2-7.onnx \
       -O /root/models/mobilenetv2/mobilenetv2-7.onnx \
  && wget -q \
       https://raw.githubusercontent.com/pytorch/hub/master/imagenet_classes.txt \
       -O /root/imagenet_classes.txt \
  && apt-get purge -y --auto-remove wget \
  && rm -rf /var/lib/apt/lists/*

# Binary CWD is /app/build so that ../AIApps/ChatServer/resource/config.json resolves correctly
WORKDIR /app
COPY --from=builder /src/build/http_server          ./build/http_server
COPY --from=builder /src/AIApps/ChatServer/resource ./AIApps/ChatServer/resource

WORKDIR /app/build
EXPOSE 8116
CMD ["./http_server", "-p", "8116"]
