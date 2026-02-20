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
	{ id: "as7341_ppfd", name: "Light (PPFD)", type: "light", unit: "µmol/m²/s", hardwareType: "as7341" },
	{ id: "vpd_calc", name: "VPD", type: "vpd", unit: "kPa", hardwareType: "calculated" },
];

const DEFAULT_TIMEZONE_OFFSET_MINUTES = 60;

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

interface SensorSim {
	value: number;
	min: number;
	max: number;
	drift: number;
	noise: number;
}

const sensorSims: Record<string, SensorSim> = {
	sht4x_temp: { value: 24.5, min: 18, max: 35, drift: 0.1, noise: 0.3 },
	sht4x_hum: { value: 55, min: 30, max: 85, drift: 0.5, noise: 1.5 },
	scd4x_co2: { value: 800, min: 400, max: 1800, drift: 15, noise: 30 },
	as7341_ppfd: { value: 450, min: 0, max: 900, drift: 5, noise: 15 },
	vpd_calc: { value: 1.1, min: 0.4, max: 2.0, drift: 0.02, noise: 0.05 },
};

function stepSensor(sim: SensorSim): number {
	sim.value += (Math.random() - 0.5) * 2 * sim.drift;
	const noisy = sim.value + (Math.random() - 0.5) * 2 * sim.noise;
	sim.value = Math.max(sim.min, Math.min(sim.max, sim.value));
	return Math.round(Math.max(sim.min, Math.min(sim.max, noisy)) * 10) / 10;
}

// --- History generation ---

interface HistoryConfig {
	intervalSec: number;
	points: number;
}

const HISTORY_CONFIG: Record<string, HistoryConfig> = {
	"12h": { intervalSec: 5 * 60, points: 144 },
	"24h": { intervalSec: 10 * 60, points: 144 },
	"7d": { intervalSec: 60 * 60, points: 168 },
};

function generateHistory(sensorId: string, range: string): Buffer {
	const config = HISTORY_CONFIG[range];
	if (!config) return Buffer.alloc(0);

	const sim = sensorSims[sensorId];
	if (!sim) return Buffer.alloc(0);

	const now = Math.floor(Date.now() / 1000);
	const startTime = now - config.points * config.intervalSec;

	const gapStart = now - Math.floor(config.points * config.intervalSec * 0.4);
	const gapEnd = gapStart + config.intervalSec * 6;

	const points: Array<{ timestamp: number; value: number }> = [];
	let simValue = sim.min + (sim.max - sim.min) * 0.3;

	for (let i = 0; i < config.points; i++) {
		const timestamp = startTime + i * config.intervalSec;

		if (timestamp >= gapStart && timestamp < gapEnd) continue;

		simValue += (Math.random() - 0.48) * sim.drift * 3;
		simValue = Math.max(sim.min, Math.min(sim.max, simValue));
		const value = simValue + (Math.random() - 0.5) * sim.noise;

		points.push({
			timestamp,
			value: Math.round(Math.max(sim.min, Math.min(sim.max, value)) * 10) / 10,
		});
	}

	// Encode as binary: uint32_le timestamp + float32_le value per point
	const buf = Buffer.alloc(points.length * 8);
	for (let i = 0; i < points.length; i++) {
		buf.writeUInt32LE(points[i].timestamp, i * 8);
		buf.writeFloatLE(points[i].value, i * 8 + 4);
	}
	return buf;
}

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

	switch (type) {
		case "ping":
			sendTo(ws, { type: "pong", timestamp: Date.now() });
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
			const sensorId = msg.sensorId as string;
			const range = msg.range as string;
			const buf = generateHistory(sensorId, range);
			if (buf.length > 0) {
				sendTo(ws, {
					type: "history",
					sensorId,
					range,
					pointSize: 8,
					count: buf.length / 8,
					data: buf.toString("base64"),
				});
			}
			break;
		}

		case "device_control": {
			const target = msg.target as string;
			const on = msg.on as boolean;
			const device = DEVICES.find((d) => d.ipAddress === target);
			if (device) {
				device.isOn = on;
				broadcast({
					type: "device_status",
					deviceId: device.id,
					target,
					on,
					success: true,
				});
			broadcast({ type: "devices", data: DEVICES });
			}
			break;
		}

		case "add_rule": {
			const rule: RuleConfig = {
				id: msg.id as string,
				name: msg.name as string,
				enabled: msg.enabled as boolean,
				type: msg.ruleType as string | undefined,
				sensorId: msg.sensorId as string | undefined,
				operator: msg.operator as string | undefined,
				threshold: msg.threshold as number | undefined,
				thresholdOff: msg.thresholdOff as number | undefined,
				useHysteresis: msg.useHysteresis as boolean | undefined,
				minRunTimeMs: msg.minRunTimeMs as number | undefined,
				onTime: msg.onTime as string | undefined,
				offTime: msg.offTime as string | undefined,
				deviceId: msg.deviceId as string,
				action: msg.action as string,
			};
			RULES.push(rule);
			broadcast({ type: "rules", data: RULES });
			console.log("[Mock] Added rule:", rule);
			break;
		}

		case "update_rule": {
			const ruleId = msg.id as string;
			const rule = RULES.find((r) => r.id === ruleId);
			if (rule) {
				Object.assign(rule, msg);
			}
			broadcast({ type: "rules", data: RULES });
			break;
		}

		case "remove_rule": {
			const idx = RULES.findIndex((r) => r.id === (msg.id as string));
			if (idx !== -1) RULES.splice(idx, 1);
			broadcast({ type: "rules", data: RULES });
			break;
		}

		case "toggle_rule": {
			const rule = RULES.find((r) => r.id === (msg.id as string));
			if (rule) rule.enabled = !rule.enabled;
			broadcast({ type: "rules", data: RULES });
			break;
		}

		case "add_device": {
			DEVICES.push({
				id: msg.id as string,
				name: msg.name as string,
				type: msg.deviceType as string,
				controlMethod: msg.controlMethod as string,
				ipAddress: msg.ipAddress as string,
				isOn: false,
				controlMode: "manual",
			});
			broadcast({ type: "devices", data: DEVICES });
			break;
		}

		case "update_device": {
			const device = DEVICES.find((d) => d.id === (msg.id as string));
			if (device) {
				if (msg.name) device.name = msg.name as string;
				if (msg.deviceType) device.type = msg.deviceType as string;
				if (msg.controlMethod) device.controlMethod = msg.controlMethod as string;
				if (msg.ipAddress) device.ipAddress = msg.ipAddress as string;
			}
			broadcast({ type: "devices", data: DEVICES });
			break;
		}

		case "remove_device": {
			const idx = DEVICES.findIndex((d) => d.id === (msg.id as string));
			if (idx !== -1) DEVICES.splice(idx, 1);
			broadcast({ type: "devices", data: DEVICES });
			break;
		}

		case "add_sensor": {
			SENSORS.push({
				id: msg.id as string,
				name: msg.name as string,
				type: msg.sensorType as string,
				unit: msg.unit as string,
				hardwareType: msg.hardwareType as string,
			});
			broadcast({ type: "sensor_config", data: SENSORS });
			break;
		}

		case "update_sensor": {
			const sensor = SENSORS.find((s) => s.id === (msg.id as string));
			if (sensor) {
				if (msg.name) sensor.name = msg.name as string;
				if (msg.sensorType) sensor.type = msg.sensorType as string;
				if (msg.unit) sensor.unit = msg.unit as string;
				if (msg.hardwareType) sensor.hardwareType = msg.hardwareType as string;
			}
			broadcast({ type: "sensor_config", data: SENSORS });
			break;
		}

		case "remove_sensor": {
			const idx = SENSORS.findIndex((s) => s.id === (msg.id as string));
			if (idx !== -1) SENSORS.splice(idx, 1);
			broadcast({ type: "sensor_config", data: SENSORS });
			break;
		}

		case "get_ppfd_calibration":
			sendTo(ws, { type: "ppfd_calibration", factor: 1.0 });
			break;

		case "calibrate_ppfd": {
			const knownPpfd = msg.knownPpfd as number;
			if (knownPpfd > 0) {
				const factor = knownPpfd / 450;
				sendTo(ws, { type: "ppfd_calibration", factor, success: true });
			} else {
				sendTo(ws, { type: "ppfd_calibration", success: false, error: "invalid_value" });
			}
			break;
		}

		case "reset_ppfd_calibration":
			sendTo(ws, { type: "ppfd_calibration", factor: 1.0, success: true });
			break;

		case "get_settings":
			sendTo(ws, { 
				type: "settings", 
				data: { 
					timezoneOffsetMinutes: DEFAULT_TIMEZONE_OFFSET_MINUTES
				} 
			});
			break;

		case "set_timezone": {
			const offsetMinutes = msg.offsetMinutes as number;
			console.log("[Mock] Set timezone offset:", offsetMinutes);
			sendTo(ws, { type: "settings", data: { timezoneOffsetMinutes: offsetMinutes } });
			break;
		}

		default:
			console.log(`[Mock] Unknown message type: ${type}`);
	}
}

// --- Periodic sensor broadcasts ---

const BROADCAST_INTERVAL = 5000;

setInterval(() => {
	const data = SENSORS.map((s) => ({
		id: s.id,
		type: s.type,
		value: stepSensor(sensorSims[s.id]),
	}));

	broadcast({ type: "sensors", data, timestamp: Date.now() });
}, BROADCAST_INTERVAL);

// --- Connection handling ---

wss.on("connection", (ws) => {
	console.log("[Mock] Client connected");

	sendTo(ws, { type: "sensor_config", data: SENSORS });
	sendTo(ws, { type: "devices", data: DEVICES });
	sendTo(ws, { type: "rules", data: RULES });

	ws.on("message", (raw) => handleMessage(ws, raw.toString()));
	ws.on("close", () => console.log("[Mock] Client disconnected"));
});

console.log(`[Mock] WebSocket server running on ws://localhost:${PORT}`);
