#!/usr/bin/env bash

set -Eeuo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd -P)
script_name=$(basename "${BASH_SOURCE[0]}")

if [ $# -eq 0 ]; then
    echo "No argument supplied, must provide a branch"
    echo "$script_dir/$script_name <branch>"
    exit 1
fi


git reset --hard
git clean -xfd
git submodule foreach --recursive git reset --hard
git submodule foreach --recursive git clean -xfd
git checkout -b tmp
git branch -D "$1"
git fetch origin "$1"
git checkout "$1"
git branch -D tmp
git submodule update