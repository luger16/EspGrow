# AGENTS.md — EspGrow Project Guidelines

**Project**: EspGrow — ESP32 grow tent monitoring & automation
**Stack**: SvelteKit 2 + Svelte 5 + TypeScript + Tailwind CSS 4 + shadcn-svelte
**Structure**: `/web` (frontend) + `/firmware` (ESP32 firmware)

---

## 1. Commands (run from `/web`)

```bash
npm run dev              # Dev server
npm run build            # Production build
npm run check            # Type check (primary quality gate)
npm run check:watch      # Type check watch mode
```

**Note**: No ESLint/Prettier/tests configured. TypeScript strict mode enforces quality.

---

## 2. Project Structure

```
/web
├── src/
│   ├── routes/             # SvelteKit routes (file-based routing)
│   │   ├── +layout.svelte  # Root layout
│   │   ├── +page.svelte    # Index page
│   │   └── layout.css      # Global styles (Tailwind + custom CSS vars)
│   ├── lib/
│   │   ├── components/ui/  # shadcn-svelte components (future)
│   │   ├── assets/         # Static assets (favicon, etc.)
│   │   ├── hooks/          # Custom Svelte hooks/utilities
│   │   ├── utils.ts        # Utility functions (cn, type helpers)
│   │   └── index.ts        # $lib barrel export
│   ├── app.html            # HTML template
│   └── app.d.ts            # Global type definitions
├── static/                 # Public assets (served at /)
├── svelte.config.js        # SvelteKit configuration
├── vite.config.ts          # Vite configuration
├── tsconfig.json           # TypeScript configuration
└── components.json         # shadcn-svelte configuration

/firmware                   # ESP32 firmware (currently empty)
```

---

## 3. Available MCP Servers

### context7

- **Tools**: `resolve-library-id`, `query-docs`
- **Use for**: Looking up official documentation for any library/framework

### svelte

- **Tools**: `list-sections`, `get-documentation`, `playground-link`, `svelte-autofixer`
- **Use for**: Svelte 5 & SvelteKit documentation, playground links, component validation

---

## 4. Code Style & Conventions

### TypeScript

- **Strict mode enabled** — All strict checks on
- **Use explicit types** for function parameters and return values
- **Avoid `any`** — Use `unknown` or proper types; `@ts-ignore` forbidden
- **Use type imports**: `import type { Foo } from './bar'`
- Exception for utility types in `utils.ts`: `// eslint-disable-next-line @typescript-eslint/no-explicit-any` acceptable

### Svelte 5 Runes (Required)

- **Use runes syntax**: `$props()`, `$state()`, `$derived()`, `$effect()`
- **Snippet syntax**: `{@render children()}` instead of `<slot />`
- **Props**: `let { children, title = "Default" } = $props();`

### Imports

- **Use `$lib` alias**: `import { cn } from '$lib/utils'`
- **Group imports**: (1) External packages (2) `$lib` (3) Relative (4) Type-only

### Component Structure

```svelte
<script lang="ts">
  import { cn } from '$lib/utils';
  import type { ComponentProps } from 'svelte';
  
  type Props = ComponentProps<'button'> & {
    variant?: 'default' | 'destructive';
  };
  let { children, variant = 'default', class: className, ...rest } = $props<Props>();
  
  let count = $state(0);
  let doubled = $derived(count * 2);
  
  $effect(() => {
    console.log('count changed:', count);
  });
</script>

<button class={cn('base-classes', className)} {...rest}>
  {@render children?.()}
</button>
```

### Styling (Tailwind CSS 4)

- **Use Tailwind utilities first** — Prefer `class="..."` over custom CSS
- **Use `cn()` utility**: `<div class={cn('base-class', isActive && 'active-class', className)} />`
- **Custom CSS variables** in `src/routes/layout.css` (shadcn-svelte theme system)
- **Dark mode** via `.dark` class
- **Use `tw-animate-css`** for animations

### File Organization

- **SvelteKit routes** use `+` prefix: `+page.svelte`, `+layout.svelte`, `+server.ts`
- **Shared components** in `$lib/components/`
- **UI primitives** (shadcn-svelte) in `$lib/components/ui/`
- **Utilities** in `$lib/utils.ts`

---

## 5. UI/UX Guidelines

- **Modern minimal design** — Clean, data-focused interface
- **Mobile-first responsive** — iOS progressive web app optimized
- **Use shadcn-svelte components** — Install via CLI when needed
- **Dark mode support** — Light/dark themes via `.dark` class
- **Semantic colors** — Use theme variables (primary, muted, destructive, accent)
- **Icons** — Lucide Svelte (`import { Thermometer } from 'lucide-svelte'`)
- **Spacing** — Tailwind scale (`p-4`, `gap-6`, `space-y-2`)
- **Typography** — `text-2xl font-bold` (headings), `text-muted-foreground` (subtle)

---

## 6. Type Safety & Best Practices

### Component Props

```typescript
type Props = {
  title: string;
  optional?: boolean;
};
let { title, optional = false } = $props<Props>();
```

### Utility Types (from `$lib/utils.ts`)

- `WithoutChild<T>`, `WithoutChildren<T>`, `WithoutChildrenOrChild<T>`, `WithElementRef<T, U>`

### Before Committing

1. Run `npm run check` to verify types
2. Run `npm run build` to verify production build
3. Match existing Tailwind patterns in `layout.css`

---

## 7. Firmware (PlatformIO + Arduino)

### Commands (run from `/firmware`)

```bash
pio run                  # Compile
pio run -t upload        # Upload to ESP32
pio device monitor       # Serial monitor (115200 baud)
pio run -t upload && pio device monitor  # Upload + monitor
```

### Project Structure

```
/firmware
├── platformio.ini       # Board & library config
├── src/
│   └── main.cpp         # Entry point (setup + loop)
├── include/             # Header files (.h)
├── lib/                 # Project-specific libraries
└── test/                # Unit tests
```

### Code Style

- **Modern C++** — Use `auto`, range-based loops, `nullptr`
- **Prefer `const`** — Mark read-only variables
- **No magic numbers** — Use `constexpr` or `#define` with clear names
- **Error handling** — Check sensor reads, handle failures gracefully
- **Serial output** — Use `Serial.println()` for debugging (115200 baud)

### File Organization

- **One class per file** — `SensorManager.h` + `SensorManager.cpp`
- **Prefix private members** — `_variableName` or `m_variableName`
- **Group by feature** — `sensors/`, `network/`, `config/`

### Libraries (via platformio.ini)

- **WiFi & WebSocket** — `AsyncTCP`, `ESPAsyncWebServer`
- **Sensors** — Adafruit unified sensor libraries (SHT4x, SCD4x, AS7341)
- **JSON** — `ArduinoJson`

### Before Uploading

1. Verify `platformio.ini` has correct board and upload port
2. Check serial monitor output for errors
3. Test WiFi connection before WebSocket setup
