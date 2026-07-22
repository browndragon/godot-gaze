#!/usr/bin/env bash
# scripts/git_tag.sh
# Automates setting and moving Git release tags locally and on remote origin.

set -euo pipefail

# Print usage if no argument is provided
if [ $# -lt 1 ]; then
    echo "Usage: $0 <tag-name> [commit-hash]"
    echo "Examples:"
    echo "  $0 v0.0.1          # Tags HEAD with v0.0.1 (deletes old remote/local tag if exists)"
    echo "  $0 v0.0.1 a1b2c3d  # Tags a specific commit with v0.0.1"
    exit 1
fi

TAG_NAME="$1"
COMMIT="${2:-HEAD}"

# Validate tag format (must start with 'v' and be followed by numbers/dots)
if [[ ! "$TAG_NAME" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.]+)?$ ]]; then
    echo "Warning: Tag name '$TAG_NAME' does not match semantic versioning format (vX.Y.Z)."
    read -p "Are you sure you want to proceed with tag '$TAG_NAME'? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
    fi
fi

# Check if git repository is dirty
if ! git diff-index --quiet HEAD --; then
    echo "Warning: You have uncommitted changes in your working directory."
    read -p "Proceed anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
    fi
fi

# Check if the tag already exists locally
if git rev-parse -q --verify "refs/tags/${TAG_NAME}" >/dev/null; then
    echo "Tag '${TAG_NAME}' already exists locally. Moving/updating it..."
    echo "Deleting local tag..."
    git tag -d "${TAG_NAME}"
    
    echo "Deleting remote tag on origin..."
    # We ignore failures here in case it doesn't exist on remote yet
    git push origin --delete "${TAG_NAME}" || true
else
    # Also attempt to delete on remote in case it exists there but not locally
    echo "Checking if tag '${TAG_NAME}' exists on origin..."
    if git ls-remote --tags origin | grep -q "refs/tags/${TAG_NAME}"; then
        echo "Tag '${TAG_NAME}' exists on origin. Deleting remote tag..."
        git push origin --delete "${TAG_NAME}" || true
    fi
fi

echo "Creating local tag '${TAG_NAME}' at ${COMMIT}..."
git tag "${TAG_NAME}" "${COMMIT}"

echo "Pushing tag '${TAG_NAME}' to origin..."
git push origin "${TAG_NAME}"

echo "Successfully set and pushed tag '${TAG_NAME}'!"
