# EspGrow — build & deploy automation
# Install: brew install just

board := "c3"

# Show available commands
default:
    @just --list

# Type-check web + compile firmware
check:
    cd web && npm run check
    cd firmware && pio run -e {{board}}

# Run web dev server
dev:
    cd web && npm run dev

# Run web dev server with mock ESP32 data
dev-mock:
    cd web && npm run dev:mock

# Build web, sync to firmware data, upload everything to ESP32
deploy board=board:
    #!/usr/bin/env bash
    set -euo pipefail

    # Build web UI
    cd web && npm run build && cd ..

    # Sync to firmware data dir (preserve user json + history)
    rm -rf firmware/data/_app firmware/data/*.html firmware/data/*.html.gz firmware/data/robots.txt
    rsync -a web/build/ firmware/data/

    # Strip originals where .gz exists (saves ~60% LittleFS space)
    # Keep index.html — needed for SPA fallback routing
    find firmware/data -name '*.gz' | while read gz; do
        original="${gz%.gz}"
        [ -f "$original" ] && [ "$(basename "$original")" != "index.html" ] && rm "$original"
    done

    # Upload filesystem, then firmware
    cd firmware && pio run -e {{board}} -t uploadfs && pio run -e {{board}} -t upload

# Deploy and open serial monitor
deploy-dev board=board: (deploy board)
    cd firmware && pio device monitor
