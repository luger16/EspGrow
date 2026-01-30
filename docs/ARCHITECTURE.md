## Architecture

### System Overview

EspGrow is a standalone monitoring and automation system. The web app runs on any device, communicating with an ESP32 microcontroller that manages sensors and controls devices via relays or/and smart power plugs/strips.

### Web Application

**Stack**
- SvelteKit 2 — File-based routing, static site generation
- Svelte 5 — Runes-based reactivity (`$state`, `$derived`, `$props`)
- Tailwind 4 — CSS-first configuration via `@theme` inline
- shadcn-svelte — Accessible UI components

**Routes**
- `/` — Dashboard with real-time sensor readings and 24h charts
- `/automation` — Automation rules management
- `/settings` — Sensor and device configuration

**State Management**

Svelte 5 runes at module level (`$state` in `.svelte.ts` files) for global state (sensor data, device state, automation rules).

No local storage - all data fetched from ESP32 in real-time.

### Firmware

**Framework**: PlatformIO (Arduino-based)

**Hardware**
- Microcontroller — Any ESP32
- Sensors (I²C) — SHT41, SCD40, AS7341
- Sensors (ADC) — Capacitive soil moisture sensor
- Device control — Direct relays, Shelly RPC, or Tasmota HTTP APIs

**Security**: No authentication (local network only, trusted environment)

**Software Components (MVP)**
- Web Server — Serves web app static files from LittleFS
- HTTP API — Device control endpoints
- WebSocket Server — Real-time sensor data broadcast
- Sensor sampling (configurable interval, default 60s)
- Basic automation engine (if sensor > X, turn on device Y)
- HTTP Client — Sends commands to Shelly/Tasmota devices (or direct GPIO for relays)

### Data Flow

**Communication Protocol**
- **Commands** (web → ESP32): HTTP POST for reliability
- **Real-time data** (ESP32 → web): WebSocket broadcast for efficiency

**Sensor Reading → Display**  
ESP32 reads sensors every 60s and broadcasts via WebSocket. The web app updates its stores and re-renders the UI in real-time.

**User Action → Device Control**  
User toggles a device in the web app → HTTP POST to ESP32 → ESP32 controls device (direct relay GPIO, Shelly RPC, or Tasmota HTTP) → ESP32 broadcasts state change via WebSocket to all connected clients.

**Automation**  
Each sensor reading triggers rule evaluation. When conditions are met (e.g., temp > 28°C), the ESP32 executes actions via HTTP and broadcasts state changes to the web app.