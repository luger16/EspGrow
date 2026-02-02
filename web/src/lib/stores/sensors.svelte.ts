import type { Sensor, SensorReading, HistoricalReading } from "$lib/types";
import { websocket } from "./websocket.svelte";

function generateHistoricalData(
	baseValue: number,
	variance: number,
	days: number = 7
): HistoricalReading[] {
	const data: HistoricalReading[] = [];
	const now = new Date();

	for (let i = days * 24; i >= 0; i--) {
		const date = new Date(now.getTime() - i * 60 * 60 * 1000);
		const randomVariance = (Math.random() - 0.5) * 2 * variance;
		const value = Math.round((baseValue + randomVariance) * 10) / 10;
		data.push({ date, value });
	}

	return data;
}

export const sensors = $state<Sensor[]>([
	{
		id: "temperature",
		name: "Temperature",
		type: "temperature",
		unit: "°C",
		hardwareType: "sht41",
	},
	{
		id: "humidity",
		name: "Humidity",
		type: "humidity",
		unit: "%",
		hardwareType: "sht41",
	},
	{
		id: "co2",
		name: "CO₂",
		type: "co2",
		unit: "ppm",
		hardwareType: "scd40",
	},
]);

export const sensorReadings = $state<Record<string, SensorReading>>({});

export function updateSensorReading(sensorId: string, value: number): void {
	sensorReadings[sensorId] = {
		sensorId,
		value: Math.round(value * 10) / 10,
		timestamp: new Date(),
	};
}

export function initSensorWebSocket(): void {
	websocket.on("sensors", (data: unknown) => {
		const msg = data as { data: Record<string, number> };
		if (msg.data) {
			Object.entries(msg.data).forEach(([key, value]) => {
				if (typeof value === "number") {
					updateSensorReading(key, value);
				}
			});
		}
	});
}

export function getSensorReading(sensorId: string): SensorReading | undefined {
	return sensorReadings[sensorId];
}

export function addSensor(sensor: Sensor): void {
	sensors.push(sensor);
}

export function removeSensor(sensorId: string): void {
	const index = sensors.findIndex((s) => s.id === sensorId);
	if (index !== -1) {
		sensors.splice(index, 1);
		delete sensorReadings[sensorId];
	}
}

export function updateSensor(sensorId: string, updates: Partial<Omit<Sensor, "id">>): void {
	const sensor = sensors.find((s) => s.id === sensorId);
	if (sensor) {
		Object.assign(sensor, updates);
	}
}

export const sensorHistory: Record<string, HistoricalReading[]> = {
	temperature: generateHistoricalData(24.5, 3, 7),
	humidity: generateHistoricalData(62, 10, 7),
	co2: generateHistoricalData(850, 150, 7),
};

export function getSensorHistory(sensorId: string): HistoricalReading[] {
	return sensorHistory[sensorId] ?? [];
}
