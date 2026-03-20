#!/bin/bash
# Initialize git repository and push contents of this folder to a GitHub remote.
# Usage:
#   chmod +x deploy_to_github.sh
#   ./deploy_to_github.sh <remote-url> <branch>
# Example:
#   ./deploy_to_github.sh git@github.com:Shivasa042/Potentiostat-Wearable-Sensor.git main

if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Usage: $0 <remote-url> <branch>"
    exit 1
fi
REMOTE=$1
BRANCH=$2

# make sure script is executed from WatchScript root
cd "$(dirname "$0")" || exit 1

if [ ! -d .git ]; then
    git init
fi

git remote remove origin 2>/dev/null || true

git remote add origin "$REMOTE"

# add all files, commit, and push
git add -A
git commit -m "Initial import of WatchScript firmware and tools"

git push -u origin "$BRANCH"

echo "Pushed to $REMOTE on branch $BRANCH"