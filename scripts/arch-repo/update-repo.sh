#!/bin/bash
# update-repo.sh: Add a .pkg.tar.zst package to the Arch Linux repository on gh-pages.
#
# Usage:
#   ./update-repo.sh <path-to.pkg.tar.zst>
#
# Environment variables:
#   APT_GPG_KEY_EMAIL  - GPG key email for signing (default: collyn094@gmail.com)
#   GITHUB_TOKEN        - GitHub token for pushing (auto-set in GitHub Actions)
#
# Works both locally (uses git push) and in GitHub Actions.
set -e

ORIG_DIR="$(pwd)"
PKG_PATH="${1:?Usage: $0 <path-to.pkg.tar.zst>}"
GPG_KEY_EMAIL="${APT_GPG_KEY_EMAIL:-collyn094@gmail.com}"

if [[ ! -f "$PKG_PATH" ]]; then
    echo "Error: File not found: $PKG_PATH"
    exit 1
fi

# Resolve absolute path to the package
PKG_ABS_PATH="$(realpath "$PKG_PATH")"
PKG_NAME="$(basename "$PKG_ABS_PATH")"
echo "=== Publishing $PKG_NAME to Arch Linux repository ==="

# ── Determine workspace ──────────────────────────────────────────────
WORKDIR="$(mktemp -d)"
trap "rm -rf '$WORKDIR'" EXIT

if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
    echo "→ CI mode: setting up gh-pages"
    REPO_URL="https://x-access-token:${GITHUB_TOKEN}@github.com/${GITHUB_REPOSITORY}.git"
    REPO_NAME="${GITHUB_REPOSITORY##*/}"
    REPO_OWNER="${GITHUB_REPOSITORY_OWNER:-collyn}"
else
    echo "→ Local mode: setting up gh-pages"
    REPO_URL="$(git remote get-url origin)"
    REPO_ROOT="$(git rev-parse --show-toplevel)"
    REPO_NAME="$(basename "$REPO_ROOT")"
    REPO_OWNER="$(echo "$REPO_URL" | sed -n 's|.*[:/]\([^/]*\)/'"$REPO_NAME"'\.git|\1|p')"
    REPO_OWNER="${REPO_OWNER:-collyn}"
fi

GITHUB_PAGES_URL="https://${REPO_OWNER}.github.io/${REPO_NAME}"

# Clone gh-pages branch
if git ls-remote --exit-code --heads origin gh-pages >/dev/null 2>&1; then
    echo "→ Cloning existing gh-pages branch..."
    git clone --depth=1 -b gh-pages "$REPO_URL" "$WORKDIR"
else
    echo "→ Creating new gh-pages branch..."
    git clone --depth=1 "$REPO_URL" "$WORKDIR"
    cd "$WORKDIR"
    git checkout --orphan gh-pages
    git rm -rf .
    cd "$ORIG_DIR"
fi

cd "$WORKDIR"

# ── Setup directory structure ─────────────────────────────────────────
ARCH_DIR="arch/x86_64"
mkdir -p "$ARCH_DIR"

# ── Copy package ───────────────────────────────────────────────────────
echo "→ Copying $PKG_NAME to $ARCH_DIR/"
cp "$PKG_ABS_PATH" "$ARCH_DIR/$PKG_NAME"

# Copy signature if available
SIG_FILE="${PKG_ABS_PATH}.sig"
if [[ -f "$SIG_FILE" ]]; then
    cp "$SIG_FILE" "$ARCH_DIR/"
    echo "✓ Signature copied: $(basename "$SIG_FILE")"
fi

# ── Generate repo database ─────────────────────────────────────────────
echo "→ Generating Arch Linux repository database..."
if ! command -v repo-add >/dev/null 2>&1; then
    echo "Error: repo-add (from pacman) is required."
    exit 1
fi

# Determine GPGKEY (fallback to email if not set)
GPGKEY="${GPGKEY:-$GPG_KEY_EMAIL}"

# Sign the database with -s, remove old same-version entries with -R
repo-add -s -R \
    "$ARCH_DIR/fcitx5-skey.db.tar.gz" \
    "$ARCH_DIR/$PKG_NAME"

# ── Export public keys ─────────────────────────────────────────────────
echo "→ Exporting public keys..."
gpg --export --armor "$GPG_KEY_EMAIL" > "key.asc" 2>/dev/null || :
gpg --export "$GPG_KEY_EMAIL" > "key.gpg" 2>/dev/null || :

# ── Generate install-arch.sh ────────────────────────────────────────────
echo "→ Generating install-arch.sh..."
cat > install-arch.sh << INSTALL_EOF
#!/bin/bash
# Install fcitx5-skey Arch Linux repository
# Usage: curl -fsSL ${GITHUB_PAGES_URL}/install-arch.sh | sudo bash
set -e

echo "Adding fcitx5-skey Arch Linux repository..."

# Download and import GPG key
curl -fsSL "${GITHUB_PAGES_URL}/key.asc" -o /tmp/fcitx5-skey-key.asc
sudo pacman-key --add /tmp/fcitx5-skey-key.asc
sudo pacman-key --lsign-key ${GPG_KEY_EMAIL}
rm -f /tmp/fcitx5-skey-key.asc

# Add repo to pacman.conf
if ! grep -q '^\[fcitx5-skey\]' /etc/pacman.conf; then
    echo "" | sudo tee -a /etc/pacman.conf > /dev/null
    echo "[fcitx5-skey]" | sudo tee -a /etc/pacman.conf > /dev/null
    echo "Server = ${GITHUB_PAGES_URL}/arch/x86_64" | sudo tee -a /etc/pacman.conf > /dev/null
    echo "SigLevel = Optional TrustAll" | sudo tee -a /etc/pacman.conf > /dev/null
fi

# Update
sudo pacman -Syu

echo "✓ fcitx5-skey repository installed!"
echo "  Install with: sudo pacman -S fcitx5-skey"
echo "  Frontends:    sudo pacman -S fcitx5-gtk fcitx5-qt"
INSTALL_EOF
chmod +x install-arch.sh

# ── Commit and push ───────────────────────────────────────────────────
echo "→ Committing changes..."

git checkout gh-pages 2>/dev/null || true

echo "→ Staging changes..."
git add -A
git config user.email "github-actions[bot]@users.noreply.github.com"
git config user.name "github-actions[bot]"

if git diff --staged --quiet; then
    echo "No changes to commit."
else
    git commit -m "Add ${PKG_NAME} to Arch Linux repository

Package: fcitx5-skey
File: arch/x86_64/${PKG_NAME}"
    echo "→ Pushing to gh-pages..."
    if git fetch origin gh-pages 2>/dev/null; then
      git rebase FETCH_HEAD 2>/dev/null || git merge FETCH_HEAD --allow-unrelated-histories --no-edit 2>/dev/null || true
    fi
    git push origin gh-pages
    echo "✓ Published to Arch Linux repository!"
fi

cd "$ORIG_DIR"

echo ""
echo "=== Done! ==="
echo "  Arch install: curl -fsSL ${GITHUB_PAGES_URL}/install-arch.sh | sudo bash"
