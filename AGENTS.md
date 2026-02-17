# AGENTS.md — EspGrow

**ESP32 grow tent monitoring & automation**
**Stack**: SvelteKit 2 + Svelte 5 + TypeScript + Tailwind 4 + shadcn-svelte (web) / PlatformIO + Arduino (firmware)

---

## 1. Commands

```bash
# From repo root (requires just: brew install just)
just build          # Build web → embed assets → compile firmware
just upload         # Build + flash to ESP32
just monitor        # Open serial monitor
just dev            # Web dev server with mock ESP32 data
just check          # Type-check web + compile firmware

# Web only (from /web)
npm run check       # svelte-check — primary quality gate
npm run build       # Production build (static site + gzip)
npm run dev:mock    # Dev server + mock WebSocket server

# Firmware only (from /firmware)
pio run -e c3               # Compile for ESP32-C3
pio run -e xiao-s3          # Compile for ESP32-S3
pio run -t upload -e c3     # Flash to device
```

**No ESLint, Prettier, or tests.** TypeScript strict mode is the quality gate.

---

## 2. Project Structure

```
/web/src/
├── routes/                    # SvelteKit file-based routing (+page.svelte, +layout.svelte)
├── lib/components/            # App components (kebab-case: sensor-card.svelte)
├── lib/components/ui/         # shadcn-svelte primitives (DO NOT edit manually)
├── lib/stores/                # Global state (*.svelte.ts with module-level $state)
├── lib/types.ts               # Shared type definitions
└── lib/utils.ts               # cn() utility, type helpers

/firmware/src/
├── main.cpp                   # Entry point (setup + loop)
├── sensors.h/cpp              # I²C sensor reading
├── automation.h/cpp           # Rule engine with hysteresis
├── device_controller.h/cpp    # Shelly Gen1/Gen2, Tasmota HTTP control
├── websocket_server.h/cpp     # WebSocket broadcast + PROGMEM web serving
├── ota_manager.h/cpp          # OTA updates (file upload + GitHub releases)
├── captive_portal.h/cpp       # WiFi setup on first boot
├── storage.h/cpp              # LittleFS config persistence
└── web_assets.h               # Auto-generated (DO NOT edit)
```

---

## 3. Web Conventions

### Svelte 5 Runes (required — no legacy syntax)

```svelte
<script lang="ts">
  import { cn } from '$lib/utils';
  import type { Sensor } from '$lib/types';

  let { sensor, onclick }: { sensor: Sensor; onclick?: () => void } = $props();
  let count = $state(0);
  let doubled = $derived(count * 2);
  $effect(() => { console.log('changed:', count); });
</script>

<!-- Tailwind first, cn() for conditionals, {@render} not <slot> -->
<button class={cn('px-4 py-2', active && 'bg-primary')} {onclick}>
  {@render children?.()}
</button>
```

- **State**: `$state()`, `$derived()`, `$effect()` — never `writable()`
- **Snippets**: `{@render children()}` — never `<slot />`
- **Styling**: Tailwind utilities + `cn()` + shadcn theme vars (`text-muted-foreground`, `bg-muted`)
- **Icons**: `@lucide/svelte` — `import { Thermometer } from '@lucide/svelte'`

### Stores

Global state in `$lib/stores/*.svelte.ts` — module-level `$state` + exported mutation functions + `init*WebSocket()` to register handlers via `websocket.on("type", handler)`.

### TypeScript

- **Strict mode** — all checks enabled
- **Explicit types** for function params and return values
- **`import type`** for type-only imports
- **No `any`** — use `unknown` + narrowing; `@ts-ignore` forbidden

### Imports (ordered)

1. External packages (`svelte`, `bits-ui`, `clsx`)
2. `$lib/` imports
3. Relative imports
4. Type-only imports last

---

## 4. Firmware Conventions

- **Namespaces** over classes: `Sensors::init()`, `Automation::loop()`, `DeviceController::control()`
- **One namespace per file pair**: `sensors.h` (declarations) + `sensors.cpp` (implementations)
- **`#pragma once`** for include guards
- **`const` by default**, `constexpr` / `#define` for constants — no magic numbers
- **Fixed-size char arrays** in structs (`char id[24]`), Arduino `String` in function signatures
- **Error handling**: check sensor reads for `NaN`, check HTTP return codes, log via `Serial.println()`
- **Libraries**: ArduinoJson 7, ESPAsyncWebServer, Sensirion I2C (SHT3x/4x, SCD4x), Adafruit AS7341
- **`web_assets.h`** is auto-generated — never edit manually

---

## 5. Architecture

**Communication**: Web → ESP32 via HTTP POST; ESP32 → Web via WebSocket broadcast. JSON messages: `{ type: "msg_type", ...payload }`.

**Build pipeline**: `npm run build` → `embed_web.py` → `pio run` = single binary with PROGMEM-embedded web UI. LittleFS is for runtime config/history only.

---

## 6. Commit Messages

Conventional commits with scope: `feat(web):`, `fix(firmware):`, `chore:`

**Before committing**: `npm run check` + `npm run build` (from `/web`), `pio run` (from `/firmware`)
