# EspGrow — build & deploy automation
# Install: brew install just

# Show available commands
default:
    @just --list

# Type-check web + compile firmware
check board:
    cd web && npm run check
    cd firmware && pio run -e {{board}}

# Run web dev server with mock ESP32 data
dev:
    cd web && npm run dev:mock

# Build web assets, embed into firmware, and compile
build board:
    cd web && npm run build
    python3 firmware/tools/embed_web.py
    cd firmware && pio run -e {{board}}

# Build and flash firmware to device
upload board: (build board)
    cd firmware && pio run -t upload -e {{board}}

# Open serial monitor
monitor:
    cd firmware && pio device monitor
