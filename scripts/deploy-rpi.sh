#!/usr/bin/env bash
# Deploy EthervoxAI to Raspberry Pi

set -euo pipefail

# Configuration
RPI_USER="${RPI_USER:-pi}"
RPI_HOST="${RPI_HOST:-raspberrypi.local}"
RPI_DIR="${RPI_DIR:-/home/pi/ethervoxai}"

echo "Deploying to ${RPI_USER}@${RPI_HOST}:${RPI_DIR}"

# Build for Raspberry Pi
echo "Building for Raspberry Pi..."
make build-rpi

# Create deployment directory on Pi
echo "Creating directory on Raspberry Pi..."
ssh "${RPI_USER}@${RPI_HOST}" "mkdir -p ${RPI_DIR}"

# Copy binary and required files
echo "Copying files..."
scp build-rpi/ethervoxai "${RPI_USER}@${RPI_HOST}:${RPI_DIR}/"
scp -r config "${RPI_USER}@${RPI_HOST}:${RPI_DIR}/" || true
scp -r plugins "${RPI_USER}@${RPI_HOST}:${RPI_DIR}/" || true

# Set executable permissions
ssh "${RPI_USER}@${RPI_HOST}" "chmod +x ${RPI_DIR}/ethervoxai"

echo "Deployment complete!"
echo "To run on Pi: ssh ${RPI_USER}@${RPI_HOST} 'cd ${RPI_DIR} && ./ethervoxai'"