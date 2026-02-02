# EspGrow — build & deploy automation
# Install: brew install just

# Default ESP32 board (c3 or xiao-s3)
board := "c3"

# Show available commands
default:
    @just --list


# ─── Web (SvelteKit) ──────────────────────────────────────────────────────────

# Build web UI (SvelteKit → static)
web-build:
    cd web && npm run build
    find web/build -name "*.br" -delete

# Run web dev server (local only)
web-dev:
    cd web && npm run dev

# Type-check web UI
web-check:
    cd web && npm run check


# ─── Filesystem (ESP data) ────────────────────────────────────────────────────

# Sync built web UI into ESP filesystem staging area
fs-sync: web-build
    rm -rf firmware/data/_app firmware/data/*.html
    rsync -av --exclude='*.json' --exclude='history/' web/build/ firmware/data/

# Upload filesystem (web UI only)
fs-upload board=board: fs-sync
    cd firmware && pio run -e {{board}} -t uploadfs


# ─── Firmware ─────────────────────────────────────────────────────────────────

# Upload firmware only (no filesystem)
fw-upload board=board:
    cd firmware && pio run -e {{board}} -t upload


# ─── Deploy / Device ──────────────────────────────────────────────────────────

# Upload firmware + filesystem
deploy board=board: fs-sync
    cd firmware && pio run -e {{board}} -t uploadfs -t upload

# Deploy and open serial monitor
deploy-dev board=board: (deploy board)
    cd firmware && pio device monitor
