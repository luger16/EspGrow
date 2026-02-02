## EspGrow

**Standalone grow tent monitoring and automation powered by ESP32**

> Self-hosted. No cloud. No subscriptions. Just your tent, your data, your control.

---

### What is EspGrow?

EspGrow is a complete monitoring and automation system for indoor grow tents that runs entirely on a cheap ESP32 microcontroller. No cloud dependency, no vendor lock-in, just a local web interface accessible from any device on your network.

### Features

- **Real-time monitoring** — Live sensor readings on a clean dashboard
- **Sensor history** — 24h/7d charts for temperature, humidity, CO₂, light, and soil moisture
- **Device control** — Manage devices (fans, lights, heaters) via smart power strips
- **Automation** — Simple rules (if sensor > X, turn on device Y)
- **Mobile-ready** — iOS-installable progressive web app

### Roadmap

- [ ] Data retention (rolling window, 7-14 days)
- [ ] Push notifications / alerts
- [ ] PWA caching (IndexedDB)
- [ ] Sensor calibration UI
- [ ] Automation history/logs
- [ ] OTA firmware updates
- [ ] Multi-condition automation rules
- [ ] Data export / config backup
- [ ] Grow journal
- [ ] SQLite database
- [ ] Customizable dashboard (sensor/device naming and reordering)

### Hardware

**Microcontroller**
Any ESP32 microcontroller

**Sensors**
- SHT41 (temperature + humidity)
- SCD40 (CO₂ + temperature + humidity)
- AS7341 (light spectrum / PPFD)
- Capacitive soil moisture sensor

**Device control**
- Direct relay modules connected to ESP32
- Smart power plugs/strips with HTTP APIs (Shelly RPC, Tasmota)

---

### License

MIT