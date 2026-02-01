export type Theme = "light" | "dark" | "system";
export type TemperatureUnit = "celsius" | "fahrenheit";

export type Settings = {
	esp32IpAddress: string;
	temperatureUnit: TemperatureUnit;
	theme: Theme;
};

const defaultSettings: Settings = {
	esp32IpAddress: "",
	temperatureUnit: "celsius",
	theme: "system",
};

export const settings = $state<Settings>({ ...defaultSettings });

export function updateSettings(updates: Partial<Settings>): void {
	Object.assign(settings, updates);
	if ("theme" in updates) {
		applyTheme(settings.theme);
	}
	// TODO: Save to ESP32 via HTTP API when connected
}

export function applyTheme(theme: Theme): void {
	if (typeof document === "undefined") return;
	const isDark =
		theme === "dark" || (theme === "system" && window.matchMedia("(prefers-color-scheme: dark)").matches);
	document.documentElement.classList.toggle("dark", isDark);
}

export function initTheme(): void {
	applyTheme(settings.theme);
	if (typeof window === "undefined") return;
	window.matchMedia("(prefers-color-scheme: dark)").addEventListener("change", () => {
		if (settings.theme === "system") {
			applyTheme("system");
		}
	});
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
