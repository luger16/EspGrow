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
	controlMode: string;
}

const DEVICES: DeviceConfig[] = [
	{ id: "fan_exhaust", name: "Exhaust Fan", type: "fan", controlMethod: "shelly_gen2", ipAddress: "192.168.1.100", isOn: true, controlMode: "automatic" },
	{ id: "light_main", name: "Grow Light", type: "light", controlMethod: "shelly_gen2", ipAddress: "192.168.1.101", isOn: true, controlMode: "manual" },
	{ id: "humidifier", name: "Humidifier", type: "humidifier", controlMethod: "tasmota", ipAddress: "192.168.1.102", isOn: false, controlMode: "automatic" },
];

// --- Automation rules ---

interface RuleConfig {
	id: string;
	name: string;
	enabled: boolean;
	type?: string;
	sensorId?: string;
	operator?: string;
	threshold?: number;
	thresholdOff?: number;
	useHysteresis?: boolean;
	minRunTimeMs?: number;
	onTime?: string;
	offTime?: string;
	deviceId: string;
	action: string;
}

const RULES: RuleConfig[] = [
	{
		id: "rule_temp_fan",
		name: "Cool when hot",
		enabled: true,
		type: "sensor",
		sensorId: "sht4x_temp",
		operator: ">",
		threshold: 28,
		thresholdOff: 26,
		useHysteresis: true,
		minRunTimeMs: 60000,
		deviceId: "fan_exhaust",
		action: "turn_on",
	},
	{
		id: "rule_hum_humid",
		name: "Humidify when dry",
		enabled: true,
		type: "sensor",
		sensorId: "sht4x_hum",
		operator: "<",
		threshold: 50,
		thresholdOff: 60,
		useHysteresis: true,
		minRunTimeMs: 30000,
		deviceId: "humidifier",
		action: "turn_on",
	},
];

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
	"24h": { intervalSec: 10 * 60, points: 144 },
	"7d": { intervalSec: 60 * 60, points: 168 },
	"30d": { intervalSec: 4 * 60 * 60, points: 180 },
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
	}, 30_000 + Math.random() * 60_000);
}
scheduleRandomAlert();

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

		case "get_rules":
			sendTo(ws, { type: "rules", data: RULES });
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

		case "add_rule": {
			const rule: RuleConfig = {
				id: payload.id as string,
				name: payload.name as string,
				enabled: payload.enabled as boolean,
				type: payload.ruleType as string | undefined,
				sensorId: payload.sensorId as string | undefined,
				operator: payload.operator as string | undefined,
				threshold: payload.threshold as number | undefined,
				thresholdOff: payload.thresholdOff as number | undefined,
				useHysteresis: payload.useHysteresis as boolean | undefined,
				minRunTimeMs: payload.minRunTimeMs as number | undefined,
				onTime: payload.onTime as string | undefined,
				offTime: payload.offTime as string | undefined,
				deviceId: payload.deviceId as string,
				action: payload.action as string,
			};
			RULES.push(rule);
			broadcast({ type: "rules", data: RULES });
			console.log("[Mock] Added rule:", rule);
			break;
		}

		case "update_rule": {
			const ruleId = payload.id as string;
			const rule = RULES.find((r) => r.id === ruleId);
			if (rule) {
				Object.assign(rule, payload);
			}
			broadcast({ type: "rules", data: RULES });
			break;
		}

		case "remove_rule": {
			const idx = RULES.findIndex((r) => r.id === (payload.id as string));
			if (idx !== -1) RULES.splice(idx, 1);
			broadcast({ type: "rules", data: RULES });
			break;
		}

		case "toggle_rule": {
			const rule = RULES.find((r) => r.id === (payload.id as string));
			if (rule) rule.enabled = !rule.enabled;
			broadcast({ type: "rules", data: RULES });
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
				controlMode: "manual",
			});
			broadcast({ type: "devices", data: DEVICES });
			break;
		}

		case "update_device": {
			const device = DEVICES.find((d) => d.id === (payload.id as string));
			if (device) {
				if (payload.name) device.name = payload.name as string;
				if (payload.deviceType) device.type = payload.deviceType as string;
				if (payload.controlMethod) device.controlMethod = payload.controlMethod as string;
				if (payload.ipAddress) device.ipAddress = payload.ipAddress as string;
			}
			broadcast({ type: "devices", data: DEVICES });
			break;
		}

		case "remove_device": {
			const idx = DEVICES.findIndex((d) => d.id === (payload.id as string));
			if (idx !== -1) DEVICES.splice(idx, 1);
			broadcast({ type: "devices", data: DEVICES });
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

		default:
			console.log(`[Mock] Unknown message type: ${type}`);
	}
}

// --- Periodic sensor broadcasts ---

const BROADCAST_INTERVAL = 5000;

setInterval(() => {
	const now = Math.floor(Date.now() / 1000);
	const data = SENSORS.map((s) => ({
		id: s.id,
		type: s.type,
		value: generateRealisticValue(s.id, now),
	}));

	broadcast({ type: "sensors", data });
}, BROADCAST_INTERVAL);

// --- Connection handling ---

wss.on("connection", (ws) => {
	console.log("[Mock] Client connected");

	sendTo(ws, { type: "sensor_config", data: SENSORS });
	sendTo(ws, { type: "devices", data: DEVICES });
	sendTo(ws, { type: "rules", data: RULES });
	sendInitialAlerts(ws);

	ws.on("message", (raw) => handleMessage(ws, raw.toString()));
	ws.on("close", () => console.log("[Mock] Client disconnected"));
});

console.log(`[Mock] WebSocket server running on ws://localhost:${PORT}`);
