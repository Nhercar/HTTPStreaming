FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Instalamos compiladores, herramientas de build y OpenCV
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libopencv-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Directorio de trabajo
WORKDIR /app

# Al iniciar, abrimos una terminal
CMD ["/bin/bash"]