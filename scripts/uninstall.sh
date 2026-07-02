#!/usr/bin/env bash
# Remove Outflank from all known install locations.
set -euo pipefail

removed=0

remove() {
    local path="$1"
    if [[ -e "$path" ]]; then
        rm -rf "$path"
        echo "  Removed: $path"
        removed=1
    fi
}

echo "Uninstalling Outflank..."

remove "${HOME}/.vst3/Outflank.vst3"
remove "${HOME}/.clap/Outflank.clap"
remove "${HOME}/.local/bin/Outflank"

if [[ $EUID -eq 0 ]]; then
    remove "/usr/lib/vst3/Outflank.vst3"
    remove "/usr/lib/clap/Outflank.clap"
    remove "/usr/local/bin/Outflank"
else
    for path in "/usr/lib/vst3/Outflank.vst3" \
                "/usr/lib/clap/Outflank.clap" \
                "/usr/local/bin/Outflank"; do
        if [[ -e "$path" ]]; then
            echo "  Skipping $path (re-run with sudo to remove)"
        fi
    done
fi

if [[ $removed -eq 0 ]]; then
    echo "  Nothing to remove."
else
    echo "Done."
fi
