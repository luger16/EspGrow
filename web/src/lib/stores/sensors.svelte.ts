import type { Sensor, SensorReading, HistoricalReading, SpectralData } from "$lib/types";
import { websocket } from "./websocket.svelte";

export const sensors = $state<Sensor[]>([]);
export const sensorReadings = $state<Record<string, SensorReading>>({});
export const sensorHistory = $state<Record<string, Record<string, HistoricalReading[]>>>({});
export const historyVersion = $state({ value: 0 });
export const spectralData = $state<{ current: SpectralData | null }>({ current: null });

const pendingReadings = new Map<string, SensorReading>();
const pendingHistory = new Map<string, HistoricalReading[]>();
let pendingSpectral: SpectralData | null = null;
let flushHandle: ReturnType<typeof setTimeout> | number | null = null;

function scheduleFlush(): void {
	if (flushHandle !== null) return;
	if (typeof requestAnimationFrame === "undefined") {
		flushHandle = setTimeout(flush, 16);
		return;
	}
	flushHandle = requestAnimationFrame(flush);
}

function flush(): void {
	flushHandle = null;
	if (pendingReadings.size > 0) {
		for (const [id, reading] of pendingReadings) {
			sensorReadings[id] = reading;
		}
		pendingReadings.clear();
	}
	if (pendingHistory.size > 0) {
		for (const [key, points] of pendingHistory) {
			const sep = key.indexOf("|");
			const sensorId = key.slice(0, sep);
			const range = key.slice(sep + 1);
			if (!sensorHistory[sensorId]) {
				sensorHistory[sensorId] = {};
			}
			sensorHistory[sensorId][range] = points;
		}
		pendingHistory.clear();
	}
	if (pendingSpectral) {
		spectralData.current = pendingSpectral;
		pendingSpectral = null;
	}
}

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

function parseTimestamp(value: unknown): Date {
	if (typeof value === "number" && Number.isFinite(value) && value > 0) {
		return new Date(value * 1000);
	}
	return new Date();
}

type HistoryRange = "6h" | "24h" | "7d";

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

export function updateSensorReading(sensorId: string, value: number, timestamp?: unknown): void {
	pendingReadings.set(sensorId, {
		sensorId,
		value: Math.round(value * 10) / 10,
		timestamp: parseTimestamp(timestamp),
	});
	scheduleFlush();
}

export function initSensorWebSocket(): void {
	websocket.on("sensor_config", (data: unknown) => {
		if (!Array.isArray(data)) return;
		const items = data.filter(
			(item): item is Sensor =>
				item &&
				typeof item === "object" &&
				typeof item.id === "string" &&
				typeof item.name === "string"
		);
		sensors.length = 0;
		sensors.push(...items);
	});

	websocket.on("sensors", (data: unknown) => {
		if (!Array.isArray(data)) return;
		const now = new Date();
		for (const reading of data) {
			if (
				reading &&
				typeof reading === "object" &&
				typeof reading.id === "string" &&
				typeof reading.value === "number"
			) {
				updateSensorReading(
					reading.id,
					reading.value,
					"timestamp" in reading ? reading.timestamp : undefined
				);

				if (Array.isArray(reading.channels)) {
					pendingSpectral = {
						channels: reading.channels as number[],
						timestamp: now,
					};
					scheduleFlush();
				}
			}
		}
	});

	websocket.on("history", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const msg = data as Record<string, unknown>;
		if (
			typeof msg.sensorId !== "string" ||
			typeof msg.range !== "string" ||
			typeof msg.payload !== "string"
		)
			return;

		let bytes: Uint8Array;
		try {
			bytes = decodeBase64(msg.payload);
		} catch {
			return;
		}
		const points = decodeHistoryPoints(bytes);

		pendingHistory.set(`${msg.sensorId}|${msg.range}`, points);
		scheduleFlush();
	});

	websocket.on("ppfd_calibration", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const msg = data as Record<string, unknown>;
		ppfdCalibration.loading = false;
		if (msg.success === false) {
			ppfdCalibration.error =
				msg.error === "no_reading"
					? "No light reading available. Make sure the AS7341 sensor is connected and the grow light is on."
					: "Invalid calibration value.";
		} else {
			ppfdCalibration.factor = typeof msg.factor === "number" ? msg.factor : 1.0;
			ppfdCalibration.error = null;
		}
	});
}

export function requestHistory(sensorId: string, range: HistoryRange, force = false): void {
	if (!force && sensorHistory[sensorId]?.[range]?.length) return;
	websocket.send("get_history", { sensorId, range });
}

const STALE_AFTER_MS: Record<HistoryRange, number> = {
	"6h": 120 * 1000,
	"24h": 600 * 1000,
	"7d": 3600 * 1000,
};

export function isHistoryStale(sensorId: string, range: HistoryRange): boolean {
	const points = sensorHistory[sensorId]?.[range];
	if (!points?.length) return true;
	const last = points[points.length - 1];
	return Date.now() - last.date.getTime() > STALE_AFTER_MS[range];
}

export function clearSensorHistory(): void {
	pendingHistory.clear();
	for (const key of Object.keys(sensorHistory)) {
		delete sensorHistory[key];
	}
	historyVersion.value++;
}

export function getSensorHistory(
	sensorId: string,
	range: HistoryRange = "7d"
): HistoricalReading[] {
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
		leafTempOffset: sensor.leafTempOffset ?? 0,
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
		leafTempOffset: updates.leafTempOffset,
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
