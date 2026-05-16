import type { DeviceType } from "$lib/types";

export const SENSOR_TYPE_COLORS: Record<string, string> = {
	temperature: "hsl(0, 72%, 51%)",
	humidity: "hsl(199, 89%, 48%)",
	co2: "hsl(220, 15%, 55%)",
	light: "hsl(45, 93%, 47%)",
	vpd: "hsl(280, 65%, 60%)",
	dewpoint: "hsl(175, 70%, 45%)",
};

export const DEVICE_TYPE_COLORS: Record<DeviceType, string> = {
	fan: "hsl(145, 45%, 55%)",
	light: "hsl(38, 70%, 55%)",
	heater: "hsl(8, 65%, 58%)",
	pump: "hsl(230, 45%, 60%)",
	humidifier: "hsl(185, 40%, 55%)",
	dehumidifier: "hsl(310, 40%, 58%)",
};

export const NORMALIZATION_RANGES: Record<string, [number, number]> = {
	temperature: [10, 40],
	dewpoint: [10, 40],
	humidity: [0, 100],
	co2: [300, 2000],
	light: [0, 1500],
	vpd: [0, 2.5],
	device: [0, 1],
};

export const SENSOR_TYPE_LABELS: Record<string, string> = {
	temperature: "Temperature",
	humidity: "Humidity",
	co2: "CO₂",
	light: "Light",
	vpd: "VPD",
	dewpoint: "Dew Point",
};

export type TimeRange = "6h" | "24h" | "7d";

export const GAP_THRESHOLDS: Record<TimeRange, number> = {
	"6h": 4 * 60 * 1000,
	"24h": 15 * 60 * 1000,
	"7d": 90 * 60 * 1000,
};

export function getSensorColor(sensor: { type: string }): string {
	return SENSOR_TYPE_COLORS[sensor.type] ?? "hsl(0, 0%, 50%)";
}

export function getDeviceColor(device: { type: DeviceType }): string {
	return DEVICE_TYPE_COLORS[device.type] ?? "hsl(0, 0%, 60%)";
}
