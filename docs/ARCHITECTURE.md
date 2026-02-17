# Architecture

> **Technical deep-dive into EspGrow's design and implementation**

---

## System Overview

EspGrow is a standalone monitoring and automation system running entirely on an ESP32 microcontroller. The web application (SvelteKit) communicates with the ESP32 over your local network—no cloud services, no external dependencies.

**Communication Model:**
- **Web → ESP32**: HTTP POST (device control, configuration)
- **ESP32 → Web**: WebSocket broadcast (real-time sensor data)

---

## Web Application

### Tech Stack

| Layer              | Technology                                 |
|--------------------|-------------------------------------------|
| Framework          | SvelteKit 2 (file-based routing, SSG)     |
| Reactivity         | Svelte 5 runes (`$state`, `$derived`)    |
| Styling            | Tailwind 4 (`@theme` inline config)      |
| UI Components      | shadcn-svelte (accessible primitives)     |
| Type Safety        | TypeScript (strict mode)                  |

### Routes

| Path          | Purpose                                    |
|---------------|--------------------------------------------|
| `/`           | Dashboard (live sensors + 12h/24h/7d charts) |
| `/automation` | Rule management (create/edit/delete)       |
| `/settings`   | Sensor and device configuration            |

### State Management

Global state uses **Svelte 5 runes** at module level (`.svelte.ts` files):

```typescript
// sensors.svelte.ts
let sensors = $state<Sensor[]>([]);

export function updateSensor(id: string, reading: SensorReading) {
  const sensor = sensors.find(s => s.id === id);
  if (sensor) sensor.reading = reading;
}

export function getSensors() { return sensors; }
```

**No local persistence** — all data is fetched from ESP32 on page load and updated via WebSocket.

---

## Firmware

### Hardware

| Component       | Models                                         |
|-----------------|-----------------------------------------------|
| Microcontroller | ESP32-C3 Super Mini, Seeed XIAO ESP32S3       |
| Temperature/RH  | SHT3x, SHT4x (I²C)                            |
| CO₂             | SCD4x (I²C)                                   |
| Light Spectrum  | AS7341 (I²C, for PPFD calculation)           |
| Device Control  | Shelly Gen1/Gen2/Plus, Tasmota (HTTP/RPC)     |

### Software Architecture

**Framework**: PlatformIO (Arduino core for ESP32)

**Components**:

| Module                 | Responsibility                                  |
|------------------------|-------------------------------------------------|
| 🌐 Web Server          | Serve embedded web app (PROGMEM gzipped assets)|
| 🔌 WebSocket Server    | Broadcast sensor data to all connected clients  |
| 📡 HTTP API            | Device control, config, OTA endpoints           |
| 🌡️ Sensor Manager      | I²C sensor reading (60s interval, configurable) |
| ⚙️ Automation Engine    | Rule evaluation with hysteresis & min run time  |
| 🔄 OTA Manager         | Firmware updates (file upload or GitHub release)|
| 📶 Captive Portal      | WiFi setup on first boot                        |
| 💾 LittleFS Storage    | Config & sensor history persistence             |

**Security**: No authentication. Designed for local network only (trusted environment).

---

## Build Pipeline

EspGrow uses a **single-binary architecture**—the web UI is embedded directly into the firmware as PROGMEM arrays.

### Build Steps

```
┌──────────────────┐
│  npm run build   │  ──→  Static site + gzipped assets
└────────┬─────────┘
         │
         ↓
┌──────────────────┐
│  embed_web.py    │  ──→  Convert .gz to C header (PROGMEM arrays)
└────────┬─────────┘
         │
         ↓
┌──────────────────┐
│    pio run       │  ──→  Compile firmware with embedded web UI
└──────────────────┘
         │
         ↓
  📦 Single .bin file (firmware + web app)
```

### Step 1: Web Build

```bash
cd web && npm run build
```

SvelteKit generates a static site in `/web/build/` with pre-compressed `.gz` files (via `precompress` adapter option).

### Step 2: Asset Embedding

```bash
python3 firmware/tools/embed_web.py
```

Converts each `.gz` file into a PROGMEM array in `firmware/src/web_assets.h`:

```cpp
const uint8_t index_html_gz[] PROGMEM = { 0x1f, 0x8b, 0x08, ... };
const WebAsset routes[] = {
  { "/", index_html_gz, sizeof(index_html_gz), "text/html" },
  ...
};
```

### Step 3: Firmware Compile

```bash
cd firmware && pio run -e c3
```

PlatformIO compiles the firmware with embedded assets. The resulting `.bin` file contains both ESP32 code and the web UI.

**Benefit**: OTA updates deliver the entire system (firmware + UI) in a single flash.

---

## Data Flow

### 📊 Sensor Reading → Display

```
┌─────────┐         ┌─────────┐         ┌────────────┐
│ Sensors │  ─────→ │  ESP32  │  ─────→ │   Browser  │
│  (I²C)  │         │  (60s)  │         │ (WebSocket)│
└─────────┘         └────┬────┘         └────────────┘
                         │
                         ↓
                    ┌─────────┐
                    │LittleFS │
                    │(history)│
                    └─────────┘
```

1. ESP32 reads sensors every 60 seconds (configurable)
2. Broadcasts JSON message via WebSocket: `{ type: "sensor_update", sensor_id: "temp_1", value: 24.5 }`
3. Web app updates Svelte stores → UI re-renders
4. ESP32 appends reading to LittleFS for historical charts

### 🔌 User Action → Device Control

```
┌──────────┐              ┌─────────┐              ┌────────┐
│ Browser  │  HTTP POST   │  ESP32  │  HTTP/RPC    │ Shelly │
│          │  ──────────→ │         │  ──────────→ │ Device │
└──────────┘              └────┬────┘              └────────┘
                               │
                               │ WebSocket
                               │ broadcast
                               ↓
                          ┌──────────┐
                          │  Browser │
                          │ (update) │
                          └──────────┘
```

1. User toggles device in web UI
2. Browser sends `POST /api/device/control` with `{ device_id: "fan_1", state: "on" }`
3. ESP32 sends HTTP request to Shelly/Tasmota device
4. ESP32 broadcasts state change via WebSocket to all connected clients

### ⚙️ Automation Rules

```
┌─────────┐      ┌──────────────────┐      ┌────────────────┐
│ Sensors │  ──→ │ Rule Evaluation  │  ──→ │ Device Control │
└─────────┘      └──────────────────┘      └────────────────┘
```

Every sensor reading triggers rule evaluation. Example rule:

```json
{
  "condition": { "sensor_id": "temp_1", "operator": ">", "value": 28 },
  "action": { "device_id": "fan_1", "state": "on" },
  "hysteresis": 1.0,
  "min_run_time": 300
}
```

**Hysteresis**: Prevents rapid on/off cycling (e.g., turn on at 28°C, turn off at 27°C)  
**Min Run Time**: Device stays on for at least N seconds after triggered

---

## Communication Protocol

### WebSocket Messages (ESP32 → Web)

| Message Type        | Payload Example                                        |
|---------------------|--------------------------------------------------------|
| `sensor_update`     | `{ sensor_id: "temp_1", value: 24.5, unit: "°C" }`    |
| `device_state`      | `{ device_id: "fan_1", state: "on" }`                  |
| `automation_triggered` | `{ rule_id: "rule_1", action: "fan_on" }`         |

### HTTP API (Web → ESP32)

| Endpoint                  | Method | Purpose                          |
|---------------------------|--------|----------------------------------|
| `/api/device/control`     | POST   | Turn device on/off               |
| `/api/config/sensors`     | GET    | List configured sensors          |
| `/api/config/devices`     | GET    | List configured devices          |
| `/api/config/automation`  | GET    | List automation rules            |
| `/api/ota/upload`         | POST   | Upload firmware file             |
| `/api/ota/github`         | POST   | Trigger GitHub release update    |

---

## File Structure

```
/web/
├── src/routes/              # SvelteKit pages (+page.svelte)
├── src/lib/components/      # Reusable UI components
├── src/lib/stores/          # Global state (*.svelte.ts)
└── src/lib/types.ts         # Shared TypeScript types

/firmware/
├── src/main.cpp             # Arduino setup() & loop()
├── src/sensors.h/cpp        # I²C sensor reading
├── src/automation.h/cpp     # Rule engine
├── src/device_controller.h/cpp  # Shelly/Tasmota HTTP client
├── src/websocket_server.h/cpp   # WebSocket broadcast
├── src/ota_manager.h/cpp    # Firmware update logic
├── src/captive_portal.h/cpp # WiFi setup portal
├── src/storage.h/cpp        # LittleFS read/write
└── src/web_assets.h         # 🚨 AUTO-GENERATED (do not edit)
```

---

## Design Decisions

### Why PROGMEM Embedding?

**Alternative**: Serve web app from LittleFS partition.

**Chosen Approach**: Embed web app into firmware as PROGMEM arrays.

**Rationale**:
- Single-binary OTA updates (no separate SPIFFS upload)
- Faster web page serving (PROGMEM read vs. LittleFS read)
- Simpler build pipeline (one `just upload` command)

### Why WebSocket for Sensor Data?

**Alternative**: Web app polls `/api/sensors` every second.

**Chosen Approach**: ESP32 broadcasts sensor data via WebSocket.

**Rationale**:
- Lower latency (immediate updates)
- Less network overhead (one connection vs. N polling requests)
- Real-time updates for all connected clients simultaneously

### Why No Authentication?

**Security Model**: Local network only, trusted environment (home/personal use).

**Future Consideration**: Optional HTTP Basic Auth for multi-user scenarios.

---

## Performance Characteristics

| Metric                  | Value                     |
|-------------------------|---------------------------|
| Sensor sampling rate    | 60s (configurable)        |
| WebSocket message size  | ~200 bytes (JSON)         |
| Web page load time      | < 500ms (PROGMEM serving) |
| OTA update duration     | ~30s (1.5MB binary)       |
| Power consumption       | ~150mA active, ~20mA idle |
