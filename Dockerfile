FROM ubuntu:22.04

# Install build tools and OpenCV
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential cmake pkg-config libopencv-dev && \
    rm -rf /var/lib/apt/lists/*

# Set workdir
WORKDIR /app

# Copy source code
COPY . .

# Expose HTTP port
EXPOSE 8080

# Only install runtime dependencies
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    libopencv-dev && \
    rm -rf /var/lib/apt/lists/*

# Expect the built binary to be mounted at /app/build/HTTPStreaming
CMD ["/app/build/HTTPStreaming"]
