# Willow ESP32-S3 Build & Flash
set dotenv-load

DOCKER_IMAGE := "willow:latest"
DOCKER_NAME := "willow-build"
PORT := env_var_or_default("PORT", "/dev/ttyACM0")

default:
    @just --list

# === SETUP ===

install-deps:
    sudo apt install -y tio dos2unix
    sudo python3 -m venv /opt/esptool-venv
    sudo /opt/esptool-venv/bin/pip install esptool
    sudo ln -sf /opt/esptool-venv/bin/esptool.py /usr/local/bin/esptool.py
    echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc


fix-script:
    dos2unix utils.sh
    chmod +x utils.sh

build-docker:
    ./utils.sh build-docker

# === BUILD ===
shell:
    ./utils.sh docker

config:
    docker run --rm -it -v "$PWD":/willow -e TERM --name {{DOCKER_NAME}} {{DOCKER_IMAGE}} \
        /bin/bash -c "./utils.sh install && ./utils.sh config"

build:
    docker run --rm -it -v "$PWD":/willow -e TERM {{DOCKER_IMAGE}} \
        /bin/bash -c "./utils.sh build"

build-dev:
    docker run --rm -it -v "$PWD":/willow -e TERM {{DOCKER_IMAGE}} \
        /bin/bash -c "./utils.sh build dev"

dist:
    docker run --rm -it -v "$PWD":/willow -e TERM {{DOCKER_IMAGE}} \
        /bin/bash -c "./utils.sh dist"

# === FLASH ===
erase:
    sudo env "PATH=$PATH" PORT={{PORT}} ./utils.sh erase-flash

# Flashear firmware completo
flash: erase
    sudo env "PATH=$PATH" PORT={{PORT}} ./utils.sh flash

# Flashear solo la app (más rápido, sin borrar todo)
flash-app:
    sudo env "PATH=$PATH" PORT={{PORT}} ./utils.sh flash-app

# Flashear dist bin
flash-dist: erase
    sudo env "PATH=$PATH" PORT={{PORT}} ./utils.sh flash-dist

# === MONITOR ===

# Abrir terminal serie
monitor:
    sudo env "PATH=$PATH" PORT={{PORT}} ./utils.sh monitor

# === UTILS ===

# Flujo completo: build + dist + flash
all: build dist flash

# Limpiar build
clean:
    docker run --rm -it -v "$PWD":/willow -e TERM {{DOCKER_IMAGE}} \
        /bin/bash -c "./utils.sh clean"

# Limpiar todo
fullclean:
    docker run --rm -it -v "$PWD":/willow -e TERM {{DOCKER_IMAGE}} \
        /bin/bash -c "./utils.sh fullclean"

# Resetear dispositivo
reset:
    sudo env "PATH=$PATH" PORT={{PORT}} ./utils.sh reset {{PORT}}
