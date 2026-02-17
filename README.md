# EspGrow

> **Self-hosted grow tent monitoring. No cloud. No subscriptions.**

## What is EspGrow?

A complete monitoring and automation system for indoor grow tents, running entirely on an ESP32 microcontroller. Monitor sensors, control devices, and automate your environment—all through a local web interface accessible from any device on your network.

**No cloud dependency. No vendor lock-in. Just your tent, your data, your control.**

## Features

🌡️ **Real-time monitoring** — Live sensor readings displayed on a clean dashboard  
📊 **Sensor history** — 12h/24h/7d charts for temperature, humidity, CO₂, and light  
🔌 **Device control** — Manage fans, lights, and heaters via Shelly or Tasmota smart plugs  
⚙️ **Automation rules** — Simple triggers (if temp > 28°C, turn on fan)  
🔄 **OTA updates** — Update firmware wirelessly via file upload or GitHub releases  
💾 **Config backup/restore** — Export and import all settings as JSON  
📱 **Mobile-ready** — Progressive web app, installable on iOS/Android

## Getting Started

### Prerequisites

- [Node.js](https://nodejs.org/) 20+
- [PlatformIO](https://platformio.org/) — `pip install platformio`
- [just](https://github.com/casey/just) — `brew install just` (or [install guide](https://github.com/casey/just#installation))

### Build & Flash

```bash
git clone https://github.com/luger16/EspGrow.git
cd EspGrow

just build    # Build web UI → embed → compile firmware
just upload   # Flash to connected ESP32
```

### First Boot

1. Connect your ESP32 via USB and run `just upload`
2. On first boot, EspGrow creates a WiFi hotspot: **`EspGrow-Setup`**
3. Connect to it and navigate to `http://192.168.4.1`
4. Enter your WiFi credentials
5. Access the dashboard at `http://espgrow.local` (or the IP shown in serial monitor)

### Development

```bash
just dev      # Start web dev server with mock sensor data
just monitor  # Open serial monitor
just check    # Type-check web + verify firmware compiles
```

## Hardware

### Microcontroller

- ESP32-C3 *(tested: ESP32-C3 Super Mini)*
- ESP32-S3 *(tested: Seeed XIAO ESP32S3)*

### Sensors (I²C)

| Sensor      | Measures                          |
|-------------|-----------------------------------|
| SHT3x/SHT4x | Temperature + Humidity            |
| SCD4x       | CO₂ + Temperature + Humidity      |
| AS7341      | Light spectrum (PPFD calculation) |

### Device Control

| Device Type            | Protocol         | Notes                        |
|------------------------|------------------|------------------------------|
| Shelly Gen1            | HTTP             | Plug S, 1PM, etc.            |
| Shelly Gen2/Plus       | RPC over HTTP    | Plus Plug S, Pro series      |
| Tasmota-flashed plugs  | HTTP             | Any Tasmota-compatible device|

## Project Structure

```
/web        SvelteKit 2 + Svelte 5 + Tailwind 4 + shadcn-svelte
/firmware   PlatformIO (Arduino framework, ESP32)
/docs       Architecture documentation
```

See [ARCHITECTURE.md](docs/ARCHITECTURE.md) for system design details.

## Troubleshooting

**WiFi setup not appearing**
- Reset the ESP32 and wait 30 seconds for the captive portal to start
- Check serial monitor (`just monitor`) for startup logs

**Sensor not detected**
- Verify I²C wiring (SDA/SCL pins correct for your board)
- Check serial logs for I²C scan results

**OTA update fails**
- Ensure ESP32 and your browser are on the same network
- Try flashing via USB instead (`just upload`)

**Device control not working**
- Verify device IP address is correct in settings
- For Shelly devices, ensure "Cloud" is disabled (local-only mode)

For more help, [open an issue](https://github.com/luger16/EspGrow/issues).

## Roadmap

- 🔜 Device state history charts
- 🔜 Power usage tracking
- 🔜 Push notifications (PWA alerts)
- 💡 Cost counter (electricity usage)
- 💡 Granular device control (SpiderFarmer grow lights, inline fans with speed control)
- 💡 GPIO relay support (direct ESP32 control)
- 💡 AP-only mode (no WiFi network required)
- 💡 Dashboard customization (reorder sensors, custom names)

## Tech Stack

**Web**: SvelteKit 2, Svelte 5 (runes), TypeScript, Tailwind 4, shadcn-svelte  
**Firmware**: PlatformIO, Arduino framework, ESPAsyncWebServer, ArduinoJson 7  
**Communication**: WebSocket (sensor data broadcast) + HTTP (device control)

## License

MIT — See [LICENSE](LICENSE) for details.
