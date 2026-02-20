import { websocket } from "./websocket.svelte";

export type Theme = "light" | "dark" | "system";
export type TemperatureUnit = "celsius" | "fahrenheit";

export type Settings = {
	temperatureUnit: TemperatureUnit;
	theme: Theme;
};

const STORAGE_KEY = "espgrow-settings";

const defaultSettings: Settings = {
	temperatureUnit: "celsius",
	theme: "system",
};

function loadSettings(): Settings {
	if (typeof localStorage === "undefined") return { ...defaultSettings };
	try {
		const stored = localStorage.getItem(STORAGE_KEY);
		if (!stored) return { ...defaultSettings };
		const parsed = JSON.parse(stored) as Partial<Settings>;
		return {
			temperatureUnit: parsed.temperatureUnit === "fahrenheit" ? "fahrenheit" : "celsius",
			theme: parsed.theme === "light" || parsed.theme === "dark" ? parsed.theme : "system",
		};
	} catch {
		return { ...defaultSettings };
	}
}

function saveSettings(s: Settings): void {
	if (typeof localStorage === "undefined") return;
	try {
		localStorage.setItem(STORAGE_KEY, JSON.stringify(s));
	} catch {
		// Storage full or unavailable — silently ignore
	}
}

export const settings = $state<Settings>(loadSettings());

export function updateSettings(updates: Partial<Settings>): void {
	Object.assign(settings, updates);
	saveSettings(settings);
	if ("theme" in updates) {
		applyTheme(settings.theme);
	}
}

export function applyTheme(theme: Theme): void {
	if (typeof document === "undefined") return;
	const isDark =
		theme === "dark" || (theme === "system" && window.matchMedia("(prefers-color-scheme: dark)").matches);
	document.documentElement.classList.toggle("dark", isDark);
}

let themeCleanup: (() => void) | null = null;

export function initTheme(): void {
	applyTheme(settings.theme);
	if (typeof window === "undefined") return;

	if (themeCleanup) {
		themeCleanup();
		themeCleanup = null;
	}

	const mediaQuery = window.matchMedia("(prefers-color-scheme: dark)");
	const handleChange = () => {
		if (settings.theme === "system") {
			applyTheme("system");
		}
	};

	mediaQuery.addEventListener("change", handleChange);
	themeCleanup = () => mediaQuery.removeEventListener("change", handleChange);
}

export function convertTemperature(celsius: number, unit: TemperatureUnit): number {
	if (unit === "fahrenheit") {
		return (celsius * 9) / 5 + 32;
	}
	return celsius;
}

export function formatTemperature(celsius: number, unit: TemperatureUnit): string {
	const value = convertTemperature(celsius, unit);
	const symbol = unit === "fahrenheit" ? "°F" : "°C";
	return `${value.toFixed(1)}${symbol}`;
}

export function initSettingsWebSocket(): void {
	websocket.send("get_settings", {});
}
