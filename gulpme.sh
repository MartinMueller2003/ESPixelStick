#!/bin/bash

# set the directory to the same value as that used to start the script
cd "$(dirname "$0")"
NODE_MODULES_PATH="$(pwd)/node_modules"

NULL_VAL="null"
NODE_VER="$NULL_VAL"
NODE_EXEC="v22.3.0/node-v22.3.0.pkg"

if command -v node &> /dev/null; then
    NODE_VER="$(node -v)"
fi

GULP_PATH="$NULL_VAL"
if command -v gulp &> /dev/null; then
    GULP_PATH="$(which gulp)"
fi

if [ "$NODE_VER" = "$NULL_VAL" ]; then
    if [ "$(id -u)" -ne 0 ]; then
        echo "This setup needs admin permissions. Please run this script with sudo."
        exit 1
    fi

    echo
    echo "Node.js is not installed! Please download and install it from:"
    echo "  http://nodejs.org/dist/$NODE_EXEC"
    echo
    echo "After you have installed Node.js, please restart this script."
    npm install
    exit 1
else
    echo "A version of Node.js ($NODE_VER) is installed. Proceeding..."
fi

InstallGulp=0

if [ "$GULP_PATH" = "$NULL_VAL" ]; then
    InstallGulp=1
    echo "no gulp path"
fi

if [ ! -d "$NODE_MODULES_PATH" ]; then
    InstallGulp=1
    echo "no modules dir"
fi

if [ "$InstallGulp" -eq 1 ]; then
    if [ "$(id -u)" -ne 0 ]; then
        echo "This setup needs admin permissions. Please run this script with sudo."
        exit 1
    fi

    echo
    echo "Install gulp"

    npm install
    npm install --global gulp-cli
    npm install --global
    npm install --save-dev gulp
    npm audit fix
else
    echo "gulp is installed. Proceeding..."
fi

echo "execute gulp"
gulp
