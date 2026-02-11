import type { Sensor, SensorReading, HistoricalReading } from "$lib/types";
import { websocket } from "./websocket.svelte";

export const sensors = $state<Sensor[]>([]);
export const sensorReadings = $state<Record<string, SensorReading>>({});
export const sensorHistory = $state<Record<string, Record<string, HistoricalReading[]>>>({});

type HistoryRange = "12h" | "24h" | "7d";

function decodeBase64(base64: string): Uint8Array {
	const binaryString = atob(base64);
	const bytes = new Uint8Array(binaryString.length);
	for (let i = 0; i < binaryString.length; i++) {
		bytes[i] = binaryString.charCodeAt(i);
	}
	return bytes;
}

function decodeHistoryPoints(data: Uint8Array): HistoricalReading[] {
	const pointSize = 8;
	const points: HistoricalReading[] = [];
	
	if (data.length < pointSize) return points;
	
	const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
	
	for (let i = 0; i + pointSize <= data.length; i += pointSize) {
		const timestamp = view.getUint32(i, true);
		const value = view.getFloat32(i + 4, true);
		
		if (timestamp > 0 && Number.isFinite(value)) {
			points.push({
				date: new Date(timestamp * 1000),
				value: Math.round(value * 10) / 10,
			});
		}
	}
	
	return points;
}

export function updateSensorReading(sensorId: string, value: number): void {
	sensorReadings[sensorId] = {
		sensorId,
		value: Math.round(value * 10) / 10,
		timestamp: new Date(),
	};
}

export function initSensorWebSocket(): void {
	websocket.on("sensor_config", (data: unknown) => {
		const msg = data as { data: Sensor[] };
		if (Array.isArray(msg.data)) {
			sensors.length = 0;
			sensors.push(...msg.data);
		}
	});

	websocket.on("sensors", (data: unknown) => {
		const msg = data as { data: Record<string, number> };
		if (msg.data) {
			Object.entries(msg.data).forEach(([sensorType, value]) => {
				if (typeof value === "number") {
					const matchingSensors = sensors.filter((s) => s.type === sensorType);
					for (const sensor of matchingSensors) {
						updateSensorReading(sensor.id, value);
					}
				}
			});
		}
	});

	websocket.on("history", (data: unknown) => {
		const msg = data as {
			sensorId: string;
			range: HistoryRange;
			data: string;
			count: number;
		};
		
		if (msg.sensorId && msg.range && msg.data) {
			const bytes = decodeBase64(msg.data);
			const points = decodeHistoryPoints(bytes);
			
			if (!sensorHistory[msg.sensorId]) {
				sensorHistory[msg.sensorId] = {};
			}
			sensorHistory[msg.sensorId][msg.range] = points;
		}
	});

	websocket.send("get_sensors", {});
}

export function getSensorReading(sensorId: string): SensorReading | undefined {
	return sensorReadings[sensorId];
}

export function requestHistory(sensorId: string, range: HistoryRange): void {
	websocket.send("get_history", { sensorId, range });
}

export function getSensorHistory(sensorId: string, range: HistoryRange = "7d"): HistoricalReading[] {
	return sensorHistory[sensorId]?.[range] ?? [];
}

export function addSensor(sensor: Sensor): void {
	websocket.send("add_sensor", {
		id: sensor.id,
		name: sensor.name,
		sensorType: sensor.type,
		unit: sensor.unit,
		hardwareType: sensor.hardwareType,
	});
}

export function removeSensor(sensorId: string): void {
	websocket.send("remove_sensor", { id: sensorId });
}

export function updateSensor(sensorId: string, updates: Partial<Omit<Sensor, "id">>): void {
	websocket.send("update_sensor", {
		id: sensorId,
		name: updates.name,
		sensorType: updates.type,
		unit: updates.unit,
		hardwareType: updates.hardwareType,
	});
}
