#!/bin/bash

IMAGE_NAME="nhcar/httpstreaming:v1"

# Permitir que Docker se conecte al servidor X de tu Ubuntu
xhost +local:docker > /dev/null

# Construir la imagen
docker build -t $IMAGE_NAME .

# Ejecutar el contenedor
docker run --rm -it \
    -v "$(pwd)":/app \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    $IMAGE_NAME
