/**
 * Mock WebSocket server that simulates the ESP32 firmware.
 * Run alongside `vite dev` to develop the frontend without hardware.
 *
 * Usage: npx tsx dev/mock-server.ts
 */

import { WebSocketServer, type WebSocket } from "ws";

const PORT = 8081;

// --- Sensor definitions (mirrors firmware sensor_config) ---

interface SensorConfig {
	id: string;
	name: string;
	type: string;
	unit: string;
	hardwareType: string;
}

const SENSORS: SensorConfig[] = [
	{ id: "sht4x_temp", name: "Temperature", type: "temperature", unit: "°C", hardwareType: "sht4x" },
	{ id: "sht4x_hum", name: "Humidity", type: "humidity", unit: "%", hardwareType: "sht4x" },
	{ id: "scd4x_co2", name: "CO₂", type: "co2", unit: "ppm", hardwareType: "scd4x" },
	{ id: "as7341_ppfd", name: "Light (PPFD)", type: "light", unit: "PPFD", hardwareType: "as7341" },
	{ id: "vpd_calc", name: "VPD", type: "vpd", unit: "kPa", hardwareType: "calculated" },
	{ id: "dewpoint_calc", name: "Dew Point", type: "dewpoint", unit: "°C", hardwareType: "calculated" },
];

// --- Devices ---

interface DeviceConfig {
	id: string;
	name: string;
	type: string;
	controlMethod: string;
	ipAddress: string;
	isOn: boolean;
	isOnline: boolean;
	controlMode: string;
	hasEnergyMonitoring?: boolean;
}

const DEVICES: DeviceConfig[] = [
	{ id: "fan_exhaust", name: "Exhaust Fan", type: "fan", controlMethod: "shelly_gen2", ipAddress: "192.168.1.100", isOn: true, isOnline: true, controlMode: "automatic", hasEnergyMonitoring: true },
	{ id: "light_main", name: "Grow Light", type: "light", controlMethod: "shelly_gen2", ipAddress: "192.168.1.101", isOn: true, isOnline: true, controlMode: "manual", hasEnergyMonitoring: true },
	{ id: "humidifier", name: "Humidifier", type: "humidifier", controlMethod: "tasmota", ipAddress: "192.168.1.102", isOn: false, isOnline: false, controlMode: "automatic", hasEnergyMonitoring: true },
];

interface EnergyState {
	deviceId: string;
	deviceName: string;
	watts: number;
	kWh: number;
	resetTimestamp: number;
}

const WATT_RANGES: Record<string, { on: [number, number]; off: [number, number] }> = {
	fan_exhaust: { on: [35, 45], off: [0, 0.5] },
	light_main: { on: [150, 200], off: [0, 0.5] },
	humidifier: { on: [25, 35], off: [0, 0.3] },
};

const energyState: EnergyState[] = DEVICES.filter((d) => d.hasEnergyMonitoring).map((d) => ({
	deviceId: d.id,
	deviceName: d.name,
	watts: 0,
	kWh: Math.round(Math.random() * 50 * 100) / 100,
	resetTimestamp: Math.floor((Date.now() - 7 * 24 * 60 * 60 * 1000) / 1000),
}));

let lastEnergyTick = Date.now();

function updateEnergyState(): void {
	const now = Date.now();
	const elapsedMs = now - lastEnergyTick;
	lastEnergyTick = now;

	for (const entry of energyState) {
		const device = DEVICES.find((d) => d.id === entry.deviceId);
		if (!device) continue;

		const range = WATT_RANGES[device.id] ?? { on: [10, 20], off: [0, 0.5] };
		const [min, max] = device.isOn ? range.on : range.off;
		entry.watts = Math.round((min + Math.random() * (max - min)) * 10) / 10;

		const kWhIncrement = (entry.watts * elapsedMs) / 3_600_000_000;
		entry.kWh = Math.round((entry.kWh + kWhIncrement) * 100) / 100;
	}
}

function syncEnergyDevices(): void {
	const energyDeviceIds = new Set(energyState.map((e) => e.deviceId));
	for (const device of DEVICES) {
		if (device.hasEnergyMonitoring && !energyDeviceIds.has(device.id)) {
			energyState.push({
				deviceId: device.id,
				deviceName: device.name,
				watts: 0,
				kWh: 0,
				resetTimestamp: Math.floor(Date.now() / 1000),
			});
		}
	}
	const activeDeviceIds = new Set(DEVICES.filter((d) => d.hasEnergyMonitoring).map((d) => d.id));
	for (let i = energyState.length - 1; i >= 0; i--) {
		if (!activeDeviceIds.has(energyState[i].deviceId)) {
			energyState.splice(i, 1);
		}
	}
	for (const entry of energyState) {
		const device = DEVICES.find((d) => d.id === entry.deviceId);
		if (device) entry.deviceName = device.name;
	}
}

// --- Device Modes ---

interface DeviceModeConfig {
	deviceId: string;
	mode: "off" | "on" | "auto" | "cycle" | "schedule";
	triggers: Array<{
		sensorType: string;
		dayThreshold: number;
		nightThreshold: number;
		deadzone: number;
		triggerAbove: boolean;
	}>;
	cycle: { onDurationSec: number; offDurationSec: number; dayOnly: boolean };
	schedule: { startTime: string; endTime: string };
}

const DEVICE_MODES: DeviceModeConfig[] = [
	{
		deviceId: "fan_exhaust",
		mode: "auto",
		triggers: [
			{ sensorType: "temperature", dayThreshold: 28, nightThreshold: 25, deadzone: 1, triggerAbove: true },
		],
		cycle: { onDurationSec: 60, offDurationSec: 60, dayOnly: false },
		schedule: { startTime: "06:00", endTime: "22:00" },
	},
	{
		deviceId: "humidifier",
		mode: "auto",
		triggers: [
			{ sensorType: "humidity", dayThreshold: 50, nightThreshold: 55, deadzone: 5, triggerAbove: false },
		],
		cycle: { onDurationSec: 60, offDurationSec: 60, dayOnly: false },
		schedule: { startTime: "06:00", endTime: "22:00" },
	},
];

interface DayNightConfig {
	useSchedule: boolean;
	dayStartTime: string;
	nightStartTime: string;
	lightThreshold: number;
	isDaytime: boolean;
}

const DAY_NIGHT_CONFIG: DayNightConfig = {
	useSchedule: false,
	dayStartTime: "06:00",
	nightStartTime: "22:00",
	lightThreshold: 50,
	isDaytime: true,
};

// --- Realistic sensor value simulation ---

function generateRealisticValue(sensorId: string, timestamp: number): number {
	const hour = new Date(timestamp * 1000).getUTCHours();
	
	switch (sensorId) {
		case 'sht4x_temp': {
			const base = 25;
			const amplitude = 7;
			const cycle = Math.sin((hour - 8) * Math.PI / 12);
			const random = (Math.random() - 0.5) * 2;
			return Math.round((base + cycle * amplitude + random * 0.5) * 10) / 10;
		}
		case 'sht4x_hum': {
			const base = 65;
			const amplitude = 20;
			const cycle = -Math.sin((hour - 8) * Math.PI / 12);
			const random = (Math.random() - 0.5) * 4;
			return Math.round(Math.max(30, Math.min(90, base + cycle * amplitude + random)) * 10) / 10;
		}
		case 'scd4x_co2': {
			const base = 800;
			const isDay = hour >= 8 && hour <= 20;
			const cycle = isDay ? -200 : 200;
			const random = (Math.random() - 0.5) * 60;
			return Math.round(Math.max(400, Math.min(1800, base + cycle + random)) * 10) / 10;
		}
		case 'as7341_ppfd': {
			const isNight = hour < 6 || hour > 20;
			if (isNight) return 0;
			const intensity = Math.sin((hour - 6) * Math.PI / 14);
			const base = intensity * 800;
			const random = (Math.random() - 0.5) * 20;
			return Math.round(Math.max(0, Math.min(900, base + random)) * 10) / 10;
		}
		case 'vpd_calc': {
			const base = 1.0;
			const amplitude = 0.5;
			const cycle = Math.sin((hour - 8) * Math.PI / 12);
			const random = (Math.random() - 0.5) * 0.1;
			return Math.round(Math.max(0.4, Math.min(2.0, base + cycle * amplitude + random)) * 100) / 100;
		}
		case 'dewpoint_calc': {
			const base = 14;
			const amplitude = 5;
			const cycle = -Math.sin((hour - 8) * Math.PI / 12);
			const random = (Math.random() - 0.5) * 0.5;
			return Math.round(Math.max(5, Math.min(25, base + cycle * amplitude + random)) * 10) / 10;
		}
		default:
			return 0;
	}
}

// --- History generation ---

interface HistoryConfig {
	intervalSec: number;
	points: number;
}

const HISTORY_CONFIG: Record<string, HistoryConfig> = {
	"6h": { intervalSec: 150, points: 144 },
	"24h": { intervalSec: 10 * 60, points: 144 },
	"7d": { intervalSec: 60 * 60, points: 168 },
};

function generateHistory(sensorId: string, range: string): Buffer {
	const config = HISTORY_CONFIG[range];
	if (!config) return Buffer.alloc(0);

	const now = Math.floor(Date.now() / 1000);
	const startTime = now - config.points * config.intervalSec;

	const gapStart = now - Math.floor(config.points * config.intervalSec * 0.4);
	const gapEnd = gapStart + config.intervalSec * 6;

	const points: Array<{ timestamp: number; value: number }> = [];

	for (let i = 0; i < config.points; i++) {
		const timestamp = startTime + i * config.intervalSec;

		if (timestamp >= gapStart && timestamp < gapEnd) continue;

		const value = generateRealisticValue(sensorId, timestamp);

		points.push({
			timestamp,
			value,
		});
	}

	const buf = Buffer.alloc(points.length * 8);
	for (let i = 0; i < points.length; i++) {
		buf.writeUInt32LE(points[i].timestamp, i * 8);
		buf.writeFloatLE(points[i].value, i * 8 + 4);
	}
	return buf;
}

// --- Climate mock data ---

const CLIMATE_CONFIG = {
	activePhase: "veg",
	dayNightMode: "auto" as "auto" | "manual",
	lightThreshold: 50,
	manualSchedule: { dayStart: "06:00", nightStart: "22:00" },
	phases: {
		seedling: {
			temp: { day: 24, night: 20 },
			humidity: { day: 70, night: 75 },
			vpd: { day: 0.5, night: 0.4 },
			co2: { day: 800, night: 600 },
		},
		veg: {
			temp: { day: 26, night: 21 },
			humidity: { day: 60, night: 65 },
			vpd: { day: 1.0, night: 0.7 },
			co2: { day: 1000, night: 600 },
		},
		flower: {
			temp: { day: 25, night: 19 },
			humidity: { day: 50, night: 55 },
			vpd: { day: 1.2, night: 0.9 },
			co2: { day: 1200, night: 600 },
		},
		dry: {
			temp: { day: 20, night: 18 },
			humidity: { day: 40, night: 45 },
			vpd: { day: 1.4, night: 1.2 },
			co2: { day: 600, night: 600 },
		},
	},
};

let alertIdCounter = 0;

function generateMockAlert(): Record<string, unknown> {
	const scenarios = [
		{ sensorId: "sht4x_temp", sensorType: "temperature", value: 31.2, target: { min: 24, max: 28 }, severity: "warning" },
		{ sensorId: "sht4x_temp", sensorType: "temperature", value: 33.5, target: { min: 24, max: 28 }, severity: "critical" },
		{ sensorId: "sht4x_hum", sensorType: "humidity", value: 82, target: { min: 55, max: 65 }, severity: "warning" },
		{ sensorId: "sht4x_hum", sensorType: "humidity", value: 38, target: { min: 55, max: 65 }, severity: "warning" },
		{ sensorId: "scd4x_co2", sensorType: "co2", value: 1650, target: { min: 800, max: 1200 }, severity: "warning" },
		{ sensorId: "scd4x_co2", sensorType: "co2", value: 1900, target: { min: 800, max: 1200 }, severity: "critical" },
		{ sensorId: "vpd_calc", sensorType: "vpd", value: 1.8, target: { min: 0.8, max: 1.2 }, severity: "warning" },
	];

	const scenario = scenarios[Math.floor(Math.random() * scenarios.length)];
	alertIdCounter++;

	return {
		id: `alert_${alertIdCounter}`,
		...scenario,
		timestamp: new Date().toISOString(),
	};
}

// Send a few initial alerts on connection, then periodic random ones
function sendInitialAlerts(ws: WebSocket): void {
	const initialAlerts = [
		{
			id: "alert_init_1",
			sensorId: "sht4x_temp",
			sensorType: "temperature",
			value: 30.4,
			target: { min: 24, max: 28 },
			severity: "warning",
			timestamp: new Date(Date.now() - 10 * 60 * 1000).toISOString(),
		},
		{
			id: "alert_init_2",
			sensorId: "sht4x_hum",
			sensorType: "humidity",
			value: 84,
			target: { min: 55, max: 65 },
			severity: "critical",
			timestamp: new Date(Date.now() - 5 * 60 * 1000).toISOString(),
		},
		{
			id: "alert_init_3",
			sensorId: "vpd_calc",
			sensorType: "vpd",
			value: 1.75,
			target: { min: 0.8, max: 1.2 },
			severity: "warning",
			timestamp: new Date(Date.now() - 2 * 60 * 1000).toISOString(),
		},
	];

	for (const alert of initialAlerts) {
		sendTo(ws, { type: "alert", data: alert });
	}
}

function scheduleRandomAlert(): void {
	setTimeout(() => {
		const alert = generateMockAlert();
		broadcast({ type: "alert", data: alert });
		console.log(`[Mock] Alert broadcast: ${alert.sensorType} ${alert.severity}`);
		scheduleRandomAlert();
	}, 120_000 + Math.random() * 180_000);
}
scheduleRandomAlert();

let eventIdCounter = 0;

function generateMockAutomationEvent(): Record<string, unknown> {
	const scenarios = [
		{ device: "Exhaust Fan", action: "turned on", mode: "auto", sensor: "Temperature", value: "29.1°C", threshold: "28°C" },
		{ device: "Exhaust Fan", action: "turned off", mode: "auto", sensor: "Temperature", value: "25.8°C", threshold: "26°C" },
		{ device: "Humidifier", action: "turned on", mode: "auto", sensor: "Humidity", value: "48%", threshold: "50%" },
		{ device: "Humidifier", action: "turned off", mode: "auto", sensor: "Humidity", value: "62%", threshold: "55%" },
	];
	const s = scenarios[Math.floor(Math.random() * scenarios.length)];
	eventIdCounter++;
	return {
		id: `auto_${eventIdCounter}`,
		title: `${s.device} (${s.mode})`,
		description: `${s.action} — ${s.sensor} at ${s.value} (threshold ${s.threshold})`,
		timestamp: Math.floor(Date.now() / 1000),
	};
}

function generateMockDeviceEvent(): Record<string, unknown> {
	const scenarios = [
		{ device: "Exhaust Fan", action: "turned on manually" },
		{ device: "Exhaust Fan", action: "turned off manually" },
		{ device: "Grow Light", action: "turned on manually" },
		{ device: "Grow Light", action: "turned off manually" },
	];
	const s = scenarios[Math.floor(Math.random() * scenarios.length)];
	eventIdCounter++;
	return {
		id: `dev_${eventIdCounter}`,
		title: s.device,
		description: s.action.charAt(0).toUpperCase() + s.action.slice(1),
		timestamp: Math.floor(Date.now() / 1000),
	};
}

function sendInitialEvents(ws: WebSocket): void {
	const now = Date.now();
	const events = [
		{ type: "automation_trigger", data: { id: "auto_init_1", title: "Exhaust Fan (auto)", description: "Turned on — Temperature at 29.3°C (threshold 28°C)", timestamp: Math.floor((now - 15 * 60 * 1000) / 1000) } },
		{ type: "device_event", data: { id: "dev_init_1", title: "Grow Light", description: "Turned on manually", timestamp: Math.floor((now - 8 * 60 * 1000) / 1000) } },
		{ type: "automation_trigger", data: { id: "auto_init_2", title: "Humidifier (auto)", description: "Turned on — Humidity at 47% (threshold 50%)", timestamp: Math.floor((now - 3 * 60 * 1000) / 1000) } },
	];
	for (const event of events) {
		sendTo(ws, event);
	}
}

function scheduleRandomEvents(): void {
	setTimeout(() => {
		const roll = Math.random();
		if (roll < 0.5) {
			const event = generateMockAutomationEvent();
			broadcast({ type: "automation_trigger", data: event });
			console.log(`[Mock] Automation event: ${event.title}`);
		} else {
			const event = generateMockDeviceEvent();
			broadcast({ type: "device_event", data: event });
			console.log(`[Mock] Device event: ${event.title}`);
		}
		scheduleRandomEvents();
	}, 60_000 + Math.random() * 120_000);
}
scheduleRandomEvents();

// --- WebSocket server ---

const wss = new WebSocketServer({ port: PORT });

function broadcast(data: Record<string, unknown>): void {
	const msg = JSON.stringify(data);
	for (const client of wss.clients) {
		if (client.readyState === 1) {
			client.send(msg);
		}
	}
}

function sendTo(ws: WebSocket, data: Record<string, unknown>): void {
	if (ws.readyState === 1) {
		ws.send(JSON.stringify(data));
	}
}

function handleMessage(ws: WebSocket, raw: string): void {
	let msg: Record<string, unknown>;
	try {
		msg = JSON.parse(raw);
	} catch {
		return;
	}

	const type = msg.type as string;
	// New protocol: payload lives under "data" key, fallback to top-level for legacy
	const payload = (typeof msg.data === "object" && msg.data !== null ? msg.data : msg) as Record<string, unknown>;

	switch (type) {
		case "ping":
			sendTo(ws, { type: "pong", data: { timestamp: Date.now() } });
			break;

		case "get_sensors":
			sendTo(ws, { type: "sensor_config", data: SENSORS });
			break;

		case "get_devices":
			sendTo(ws, { type: "devices", data: DEVICES });
			break;

		case "get_device_modes":
			sendTo(ws, { type: "device_modes", data: DEVICE_MODES });
			break;

		case "get_daynight_config":
			DAY_NIGHT_CONFIG.isDaytime = new Date().getUTCHours() >= 6 && new Date().getUTCHours() <= 20;
			sendTo(ws, { type: "daynight_config", data: DAY_NIGHT_CONFIG });
			break;

		case "get_history": {
			const sensorId = payload.sensorId as string;
			const range = payload.range as string;
			const buf = generateHistory(sensorId, range);
			if (buf.length > 0) {
				sendTo(ws, {
					type: "history",
					data: {
						sensorId,
						range,
						pointSize: 8,
						count: buf.length / 8,
						payload: buf.toString("base64"),
					},
				});
			}
			break;
		}

		case "device_control": {
			const target = payload.target as string;
			const on = payload.on as boolean;
			const device = DEVICES.find((d) => d.ipAddress === target);
			if (device) {
				device.isOn = on;
				broadcast({
					type: "device_status",
					data: {
						deviceId: device.id,
						target,
						on,
						success: true,
					},
				});
			broadcast({ type: "devices", data: DEVICES });
			}
			break;
		}

		case "set_device_mode": {
			const deviceId = payload.deviceId as string;
			const idx = DEVICE_MODES.findIndex((m) => m.deviceId === deviceId);
			const config: DeviceModeConfig = {
				deviceId,
				mode: payload.mode as DeviceModeConfig["mode"],
				triggers: (payload.triggers as DeviceModeConfig["triggers"]) ?? [],
				cycle: (payload.cycle as DeviceModeConfig["cycle"]) ?? { onDurationSec: 60, offDurationSec: 60, dayOnly: false },
				schedule: (payload.schedule as DeviceModeConfig["schedule"]) ?? { startTime: "06:00", endTime: "22:00" },
			};
			if (idx !== -1) {
				DEVICE_MODES[idx] = config;
			} else {
				DEVICE_MODES.push(config);
			}
			broadcast({ type: "device_modes", data: DEVICE_MODES });
			console.log("[Mock] Set device mode:", config.deviceId, config.mode);
			break;
		}

		case "delete_device_mode": {
			const idx = DEVICE_MODES.findIndex((m) => m.deviceId === (payload.deviceId as string));
			if (idx !== -1) DEVICE_MODES.splice(idx, 1);
			broadcast({ type: "device_modes", data: DEVICE_MODES });
			break;
		}

		case "set_daynight_config": {
			if (typeof payload.useSchedule === "boolean") DAY_NIGHT_CONFIG.useSchedule = payload.useSchedule;
			if (typeof payload.dayStartTime === "string") DAY_NIGHT_CONFIG.dayStartTime = payload.dayStartTime as string;
			if (typeof payload.nightStartTime === "string") DAY_NIGHT_CONFIG.nightStartTime = payload.nightStartTime as string;
			if (typeof payload.lightThreshold === "number") DAY_NIGHT_CONFIG.lightThreshold = payload.lightThreshold as number;
			DAY_NIGHT_CONFIG.isDaytime = new Date().getUTCHours() >= 6 && new Date().getUTCHours() <= 20;
			broadcast({ type: "daynight_config", data: DAY_NIGHT_CONFIG });
			break;
		}

		case "add_device": {
			DEVICES.push({
				id: payload.id as string,
				name: payload.name as string,
				type: payload.deviceType as string,
				controlMethod: payload.controlMethod as string,
				ipAddress: payload.ipAddress as string,
				isOn: false,
				isOnline: true,
				controlMode: "manual",
				hasEnergyMonitoring: (payload.hasEnergyMonitoring as boolean) ?? false,
			});
			syncEnergyDevices();
			broadcast({ type: "devices", data: DEVICES });
			broadcast({ type: "energy", data: energyState });
			break;
		}

		case "update_device": {
			const device = DEVICES.find((d) => d.id === (payload.id as string));
			if (device) {
				if (payload.name) device.name = payload.name as string;
				if (payload.deviceType) device.type = payload.deviceType as string;
				if (payload.controlMethod) device.controlMethod = payload.controlMethod as string;
				if (payload.ipAddress) device.ipAddress = payload.ipAddress as string;
				if (typeof payload.hasEnergyMonitoring === "boolean") device.hasEnergyMonitoring = payload.hasEnergyMonitoring;
			}
			syncEnergyDevices();
			broadcast({ type: "devices", data: DEVICES });
			broadcast({ type: "energy", data: energyState });
			break;
		}

		case "remove_device": {
			const idx = DEVICES.findIndex((d) => d.id === (payload.id as string));
			if (idx !== -1) DEVICES.splice(idx, 1);
			syncEnergyDevices();
			broadcast({ type: "devices", data: DEVICES });
			broadcast({ type: "energy", data: energyState });
			break;
		}

		case "add_sensor": {
			SENSORS.push({
				id: payload.id as string,
				name: payload.name as string,
				type: payload.sensorType as string,
				unit: payload.unit as string,
				hardwareType: payload.hardwareType as string,
			});
			broadcast({ type: "sensor_config", data: SENSORS });
			break;
		}

		case "update_sensor": {
			const sensor = SENSORS.find((s) => s.id === (payload.id as string));
			if (sensor) {
				if (payload.name) sensor.name = payload.name as string;
				if (payload.sensorType) sensor.type = payload.sensorType as string;
				if (payload.unit) sensor.unit = payload.unit as string;
				if (payload.hardwareType) sensor.hardwareType = payload.hardwareType as string;
			}
			broadcast({ type: "sensor_config", data: SENSORS });
			break;
		}

		case "remove_sensor": {
			const idx = SENSORS.findIndex((s) => s.id === (payload.id as string));
			if (idx !== -1) SENSORS.splice(idx, 1);
			broadcast({ type: "sensor_config", data: SENSORS });
			break;
		}

		case "get_ppfd_calibration":
			sendTo(ws, { type: "ppfd_calibration", data: { factor: 1.0 } });
			break;

		case "get_climate_config":
			sendTo(ws, { type: "climate_config", data: CLIMATE_CONFIG });
			break;

		case "get_climate_status": {
			const hour = new Date().getUTCHours();
			const isDay = hour >= 6 && hour <= 20;
			const phase = CLIMATE_CONFIG.activePhase as "seedling" | "veg" | "flower" | "dry";

			sendTo(ws, {
				type: "climate_status",
				data: {
					isDay,
					healthScore: 72 + Math.round(Math.random() * 20),
					currentTargets: CLIMATE_CONFIG.phases[phase],
					sensorStatuses: {
						sht4x_temp: "warning",
						sht4x_hum: "optimal",
						scd4x_co2: "optimal",
						vpd_calc: "warning",
					},
				},
			});
			break;
		}

		case "calibrate_ppfd": {
			const knownPpfd = payload.knownPpfd as number;
			if (knownPpfd > 0) {
				const factor = knownPpfd / 450;
				sendTo(ws, { type: "ppfd_calibration", data: { factor, success: true } });
			} else {
				sendTo(ws, { type: "ppfd_calibration", data: { success: false, error: "invalid_value" } });
			}
			break;
		}

		case "reset_ppfd_calibration":
			sendTo(ws, { type: "ppfd_calibration", data: { factor: 1.0, success: true } });
			break;

		case "get_energy":
			updateEnergyState();
			sendTo(ws, { type: "energy", data: energyState });
			break;

		case "reset_energy": {
			const resetDeviceId = payload.deviceId as string | undefined;
			const now = Math.floor(Date.now() / 1000);
			for (const entry of energyState) {
				if (!resetDeviceId || entry.deviceId === resetDeviceId) {
					entry.kWh = 0;
					entry.resetTimestamp = now;
				}
			}
			updateEnergyState();
			broadcast({ type: "energy", data: energyState });
			console.log(`[Mock] Energy reset${resetDeviceId ? ` for ${resetDeviceId}` : " (all)"}`);
			break;
		}

		default:
			console.log(`[Mock] Unknown message type: ${type}`);
	}
}

// --- Periodic sensor broadcasts ---

const BROADCAST_INTERVAL = 5000;

setInterval(() => {
	const now = Math.floor(Date.now() / 1000);

	// Generate temp & humidity first so derived sensors use the same values
	const values: Record<string, number> = {};
	for (const s of SENSORS) {
		if (s.id === "vpd_calc" || s.id === "dewpoint_calc") continue;
		values[s.id] = generateRealisticValue(s.id, now);
	}

	// Derive VPD from actual temp + humidity (Magnus-Tetens)
	const temp = values["sht4x_temp"];
	const hum = values["sht4x_hum"];
	if (temp !== undefined && hum !== undefined) {
		const svp = 0.6108 * Math.exp((17.27 * temp) / (temp + 237.3));
		values["vpd_calc"] = Math.round(svp * (1 - hum / 100) * 100) / 100;
		values["dewpoint_calc"] = Math.round((237.3 * (Math.log(hum / 100) + (17.27 * temp) / (temp + 237.3))) / (17.27 - (Math.log(hum / 100) + (17.27 * temp) / (temp + 237.3))) * 10) / 10;
	}

	const data = SENSORS.map((s) => {
		const entry: Record<string, unknown> = {
			id: s.id,
			type: s.type,
			value: values[s.id] ?? 0,
		};

		if (s.id === "as7341_ppfd") {
			entry.channels = [180, 420, 310, 280, 260, 220, 580, 510];
		}

		return entry;
	});

	broadcast({ type: "sensors", data });

	updateEnergyState();
	broadcast({ type: "energy", data: energyState });
}, BROADCAST_INTERVAL);

// --- Connection handling ---

wss.on("connection", (ws) => {
	console.log("[Mock] Client connected");

	sendTo(ws, { type: "sensor_config", data: SENSORS });
	sendTo(ws, { type: "devices", data: DEVICES });
	sendTo(ws, { type: "device_modes", data: DEVICE_MODES });
	sendTo(ws, { type: "daynight_config", data: { ...DAY_NIGHT_CONFIG, isDaytime: new Date().getUTCHours() >= 6 && new Date().getUTCHours() <= 20 } });
	sendInitialAlerts(ws);
	sendInitialEvents(ws);

	ws.on("message", (raw) => handleMessage(ws, raw.toString()));
	ws.on("close", () => console.log("[Mock] Client disconnected"));
});

console.log(`[Mock] WebSocket server running on ws://localhost:${PORT}`);
