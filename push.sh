#!/usr/bin/env bash
# Pushes the local cart-game repo to your GitHub account and triggers the
# cloud build of cartchaos.3dsx (GitHub Actions).
#
# Usage:  bash push.sh
# You will be asked for:
#   1. your GitHub username
#   2. the repository name you created at https://github.com/new
#   3. your password  ->  paste a GitHub Personal Access Token instead
#      (create one at https://github.com/settings/tokens  with the "repo" scope)
set -e
cd "$(dirname "$0")"

read -rp "GitHub username: " USER
read -rp "Repository name (e.g. cartchaos-3ds): " REPO

REMOTE="https://github.com/${USER}/${REPO}.git"

# Avoid clobbering an existing remote named origin.
if git remote get-url origin >/dev/null 2>&1; then
  git remote set-url origin "$REMOTE"
else
  git remote add origin "$REMOTE"
fi

git branch -M main
echo "Pushing to $REMOTE ..."
echo "(when prompted for a password, paste your GitHub token)"
git push -u origin main

echo
echo "=============================================================="
echo " PUSH DONE. Now let GitHub build the game:"
echo "   1. Open  https://github.com/${USER}/${REPO}/actions"
echo "   2. Wait for 'Build 3DS' to finish (green check, ~1-2 min)"
echo "   3. Download the 'cartchaos-3dsx' artifact (has .3dsx + .smdh)"
echo "   4. Copy both files to SD:/3ds/CartChaos/ on your 3DS"
echo "=============================================================="
