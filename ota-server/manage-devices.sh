#!/bin/bash

# This script helps manage devices without needing to enter the container

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    echo "Docker is not running or you don't have permission to use it."
    exit 1
fi

# Check if container is running
CONTAINER_NAME="ota-update-server"
if ! docker ps | grep -q $CONTAINER_NAME; then
    echo "OTA Update Server container is not running."
    echo "Start it with: ./start.sh"
    exit 1
fi

# Forward all arguments to the admin_tools.py script inside the container
docker exec -it $CONTAINER_NAME python admin_tools.py "$@"