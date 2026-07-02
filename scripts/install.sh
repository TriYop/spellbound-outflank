#!/usr/bin/env bash
# Install Outflank plugins and standalone app.
# Usage:
#   ./install.sh           — install to user directories (no root needed)
#   ./install.sh --system  — install system-wide to /usr/lib (requires sudo)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SYSTEM=0

for arg in "$@"; do
    case "$arg" in
        --system) SYSTEM=1 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

if [[ $SYSTEM -eq 1 ]]; then
    VST3_DIR="/usr/lib/vst3"
    CLAP_DIR="/usr/lib/clap"
    BIN_DIR="/usr/local/bin"
else
    VST3_DIR="${HOME}/.vst3"
    CLAP_DIR="${HOME}/.clap"
    BIN_DIR="${HOME}/.local/bin"
fi

echo "Installing Outflank..."

mkdir -p "${VST3_DIR}"
rm -rf   "${VST3_DIR}/Outflank.vst3"
cp -r    "${SCRIPT_DIR}/VST3/Outflank.vst3" "${VST3_DIR}/"
echo "  VST3  → ${VST3_DIR}/Outflank.vst3"

mkdir -p "${CLAP_DIR}"
cp       "${SCRIPT_DIR}/CLAP/Outflank.clap" "${CLAP_DIR}/"
chmod    755 "${CLAP_DIR}/Outflank.clap"
echo "  CLAP  → ${CLAP_DIR}/Outflank.clap"

mkdir -p "${BIN_DIR}"
cp       "${SCRIPT_DIR}/bin/Outflank" "${BIN_DIR}/"
chmod    755 "${BIN_DIR}/Outflank"
echo "  App   → ${BIN_DIR}/Outflank"

echo "Done."
