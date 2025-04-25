#!/bin/bash

# Check if .env file exists, if not create from example
if [ ! -f .env ]; then
    echo "Creating .env file from .env.example..."
    cp .env.example .env
    echo "Please edit .env file with your configuration"
    exit 1
fi

# Check if firmware directory exists, if not create it
if [ ! -d firmware ]; then
    echo "Creating firmware directory..."
    mkdir -p firmware
fi

# Check if devices.json exists, if not create empty one
if [ ! -f devices.json ]; then
    echo "Creating empty devices.json..."
    echo "{}" > devices.json
fi

# Start Docker Compose in detached mode
echo "Starting OTA Update Server..."
docker-compose up -d

echo "OTA Update Server is now running!"
echo "You can view logs with: docker-compose logs -f"