import type { Sensor, SensorReading, HistoricalReading } from "$lib/types";
import { websocket } from "./websocket.svelte";

export const sensors = $state<Sensor[]>([]);
export const sensorReadings = $state<Record<string, SensorReading>>({});
export const sensorHistory = $state<Record<string, Record<string, HistoricalReading[]>>>({});

interface PpfdCalibration {
	factor: number;
	loading: boolean;
	error: string | null;
}

export const ppfdCalibration = $state<PpfdCalibration>({
	factor: 1.0,
	loading: false,
	error: null,
});

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
		if (!Array.isArray(data)) return;
		const items = data.filter(
			(item): item is Sensor =>
				item && typeof item === "object" && typeof item.id === "string" && typeof item.name === "string"
		);
		sensors.length = 0;
		sensors.push(...items);
	});

	websocket.on("sensors", (data: unknown) => {
		if (!Array.isArray(data)) return;
		for (const reading of data) {
			if (
				reading && typeof reading === "object" &&
				typeof reading.id === "string" &&
				typeof reading.value === "number"
			) {
				updateSensorReading(reading.id, reading.value);
			}
		}
	});

	websocket.on("history", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const msg = data as Record<string, unknown>;
		if (typeof msg.sensorId !== "string" || typeof msg.range !== "string" || typeof msg.payload !== "string") return;
		
		const bytes = decodeBase64(msg.payload);
		const points = decodeHistoryPoints(bytes);
		
		if (!sensorHistory[msg.sensorId]) {
			sensorHistory[msg.sensorId] = {};
		}
		sensorHistory[msg.sensorId][msg.range as HistoryRange] = points;
	});

	websocket.on("ppfd_calibration", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const msg = data as Record<string, unknown>;
		ppfdCalibration.loading = false;
		if (msg.success === false) {
			ppfdCalibration.error = msg.error === "no_reading" 
				? "No light reading available. Make sure the AS7341 sensor is connected and the grow light is on."
				: "Invalid calibration value.";
		} else {
			ppfdCalibration.factor = typeof msg.factor === "number" ? msg.factor : 1.0;
			ppfdCalibration.error = null;
		}
	});

	websocket.send("get_sensors");
	websocket.send("get_ppfd_calibration");
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
		address: sensor.address ?? "",
		tempSourceId: sensor.tempSourceId ?? "",
		humSourceId: sensor.humSourceId ?? "",
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
		address: updates.address ?? "",
		tempSourceId: updates.tempSourceId ?? "",
		humSourceId: updates.humSourceId ?? "",
	});
}

export function calibratePpfd(knownPpfd: number): void {
	ppfdCalibration.loading = true;
	ppfdCalibration.error = null;
	websocket.send("calibrate_ppfd", { knownPpfd });
}

export function resetPpfdCalibration(): void {
	ppfdCalibration.loading = true;
	ppfdCalibration.error = null;
	websocket.send("reset_ppfd_calibration", {});
}
