#!/bin/bash
# update-repo.sh: Add an .rpm package to the RPM repository on the gh-pages branch.
#
# Usage:
#   ./update-repo.sh <path-to.rpm> <distro>
#
#   distro: fedora | opensuse
#
# Environment variables:
#   APT_GPG_KEY_EMAIL  - GPG key email for signing (default: collyn094@gmail.com)
#   GITHUB_TOKEN        - GitHub token for pushing (auto-set in GitHub Actions)
#
# Works both locally (uses git push) and in GitHub Actions.
set -e

ORIG_DIR="$(pwd)"
RPM_PATH="${1:?Usage: $0 <path-to.rpm> <distro>}"
DISTRO="${2:?Usage: $0 <path-to.rpm> <distro>}"
GPG_KEY_EMAIL="${APT_GPG_KEY_EMAIL:-collyn094@gmail.com}"

if [[ ! -f "$RPM_PATH" ]]; then
    echo "Error: File not found: $RPM_PATH"
    exit 1
fi

if [[ "$DISTRO" != "fedora" && "$DISTRO" != "opensuse" ]]; then
    echo "Error: distro must be 'fedora' or 'opensuse', got '$DISTRO'"
    exit 1
fi

# Resolve absolute path to the .rpm
RPM_ABS_PATH="$(realpath "$RPM_PATH")"
RPM_NAME="$(basename "$RPM_ABS_PATH")"
echo "=== Publishing $RPM_NAME to RPM repository ($DISTRO) ==="

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
RPM_DIR="rpm/${DISTRO}"
mkdir -p "$RPM_DIR"

# ── Copy .rpm ──────────────────────────────────────────────────────────
echo "→ Copying $RPM_NAME to $RPM_DIR/"
cp "$RPM_ABS_PATH" "$RPM_DIR/$RPM_NAME"

# ── Generate repo metadata with createrepo_c ──────────────────────────
echo "→ Generating RPM repository metadata..."
if ! command -v createrepo_c >/dev/null 2>&1; then
    echo "Error: createrepo_c is required. Install with:"
    echo "  dnf install createrepo_c    # Fedora"
    echo "  zypper install createrepo_c # openSUSE"
    exit 1
fi
createrepo_c "$RPM_DIR"

# ── Sign repo metadata ─────────────────────────────────────────────────
echo "→ Signing repository metadata with GPG ($GPG_KEY_EMAIL)..."
if ! gpg --list-secret-keys "$GPG_KEY_EMAIL" >/dev/null 2>&1; then
    echo "Error: GPG key '$GPG_KEY_EMAIL' not found. Import it first:"
    echo "  gpg --import /path/to/private-key.asc"
    exit 1
fi

gpg --batch --yes --detach-sign --armor \
    --local-user "$GPG_KEY_EMAIL" \
    -o "$RPM_DIR/repodata/repomd.xml.asc" \
    "$RPM_DIR/repodata/repomd.xml"

# ── Export public keys ─────────────────────────────────────────────────
echo "→ Exporting public keys..."
gpg --export --armor "$GPG_KEY_EMAIL" > "key.asc"
gpg --export "$GPG_KEY_EMAIL" > "key.gpg"

# Get key fingerprint for install scripts
FINGERPRINT=$(gpg --fingerprint --with-colons "$GPG_KEY_EMAIL" 2>/dev/null | \
    grep '^fpr:' | head -1 | cut -d: -f10)
FINGERPRINT="${FINGERPRINT:-PLACEHOLDER}"

# ── Generate install scripts ───────────────────────────────────────────

# install-fedora.sh
echo "→ Generating install-fedora.sh..."
cat > install-fedora.sh << INSTALL_EOF
#!/bin/bash
# Install fcitx5-skey RPM repository (Fedora)
# Usage: curl -fsSL ${GITHUB_PAGES_URL}/install-fedora.sh | sudo bash
set -e

echo "Adding fcitx5-skey RPM repository for Fedora..."

# Import GPG key
sudo rpm --import ${GITHUB_PAGES_URL}/key.gpg

# Add repo
cat << 'REPO' | sudo tee /etc/yum.repos.d/fcitx5-skey.repo > /dev/null
[fcitx5-skey]
name=fcitx5-skey — Vietnamese SKey input method
baseurl=${GITHUB_PAGES_URL}/rpm/fedora/
enabled=1
gpgcheck=1
repo_gpgcheck=1
gpgkey=${GITHUB_PAGES_URL}/key.gpg
REPO

echo "✓ fcitx5-skey repository installed!"
echo "  Install with: sudo dnf install fcitx5-skey"
echo "  Frontends:    sudo dnf install fcitx5-gtk fcitx5-qt"
INSTALL_EOF
chmod +x install-fedora.sh

# install-opensuse.sh
echo "→ Generating install-opensuse.sh..."
cat > install-opensuse.sh << INSTALL_EOF
#!/bin/bash
# Install fcitx5-skey RPM repository (openSUSE)
# Usage: curl -fsSL ${GITHUB_PAGES_URL}/install-opensuse.sh | sudo bash
set -e

echo "Adding fcitx5-skey RPM repository for openSUSE..."

# Import GPG key
sudo rpm --import ${GITHUB_PAGES_URL}/key.gpg

# Add repo
sudo zypper addrepo --refresh --check --gpgcheck \
    ${GITHUB_PAGES_URL}/rpm/opensuse/ fcitx5-skey

echo "✓ fcitx5-skey repository installed!"
echo "  Install with: sudo zypper install fcitx5-skey"
echo "  Frontends:    sudo zypper install fcitx5-gtk"
INSTALL_EOF
chmod +x install-opensuse.sh

# ── Commit and push ───────────────────────────────────────────────────
echo "→ Committing changes..."

git checkout gh-pages 2>/dev/null || true

git add .
git config user.email "github-actions[bot]@users.noreply.github.com"
git config user.name "github-actions[bot]"

if git diff --staged --quiet; then
    echo "No changes to commit."
else
    git commit -m "Add ${RPM_NAME} to RPM repository (${DISTRO})

Package: fcitx5-skey
File: rpm/${DISTRO}/${RPM_NAME}"
    echo "→ Pushing to gh-pages..."
    git pull --rebase origin gh-pages 2>/dev/null || git fetch origin gh-pages --depth=2 && git merge origin/gh-pages -m "Merge gh-pages"
    git push origin gh-pages
    echo "✓ Published to RPM repository (${DISTRO})!"
fi

cd "$ORIG_DIR"

echo ""
echo "=== Done! ==="
echo "  Fedora install:    curl -fsSL ${GITHUB_PAGES_URL}/install-fedora.sh | sudo bash"
echo "  openSUSE install:  curl -fsSL ${GITHUB_PAGES_URL}/install-opensuse.sh | sudo bash"
