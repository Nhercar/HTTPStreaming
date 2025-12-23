#!/bin/bash

# Definir la imagen y el contenedor
IMAGE_NAME="nhcar/httpstreaming:v1"
CONTAINER_NAME="httpstreaming"

# Obtener la ruta absoluta del directorio del script
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Construir la imagen Docker
sudo docker build -t $IMAGE_NAME "$DIR"

# Detener y eliminar cualquier contenedor anterior con el mismo nombre
if sudo docker ps -a --format '{{.Names}}' | grep -Eq "^$CONTAINER_NAME$"; then
    sudo docker stop $CONTAINER_NAME
    sudo docker rm $CONTAINER_NAME
fi

# Ejecutar el contenedor
# Si existe /dev/video0, lo pasa al contenedor para acceso a webcam
if [ -e /dev/video0 ]; then
    sudo docker run \
        --name $CONTAINER_NAME \
        --device=/dev/video0 \
        -p 8080:8080 \
        $IMAGE_NAME
else
    sudo docker run \
        --name $CONTAINER_NAME \
        -p 8080:8080 \
        $IMAGE_NAME
fi
