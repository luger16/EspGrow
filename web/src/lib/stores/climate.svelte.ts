import type {
	ClimateConfig,
	ClimateAlert,
	ClimateStatus,
	ClimatePhase,
	PhaseTargets,
	SensorStatus,
	SensorType,
	SystemEvent,
	SystemEventType,
} from "$lib/types";
import { DEFAULT_CLIMATE_CONFIG, DEFAULT_PHASE_TARGETS } from "$lib/climate-presets";
import { websocket } from "./websocket.svelte";
import { sensors, sensorReadings } from "./sensors.svelte";

const SENSOR_TYPE_LABELS: Record<SensorType, string> = {
	temperature: "Temperature",
	humidity: "Humidity",
	co2: "CO₂",
	vpd: "VPD",
	light: "Light",
	dewpoint: "Dew Point",
};

const SENSOR_TYPE_UNITS: Record<SensorType, string> = {
	temperature: "°C",
	humidity: "%",
	co2: "ppm",
	vpd: "kPa",
	light: "PPFD",
	dewpoint: "°C",
};

function formatAlert(alert: ClimateAlert): { title: string; description: string } {
	const label = SENSOR_TYPE_LABELS[alert.sensorType] ?? alert.sensorType;
	const unit = SENSOR_TYPE_UNITS[alert.sensorType] ?? "";
	const direction = alert.value > alert.target.max ? "high" : "low";
	const precision = alert.sensorType === "vpd" || alert.sensorType === "dewpoint" ? 2
		: alert.sensorType === "temperature" ? 1 : 0;
	const valueStr = `${alert.value.toFixed(precision)}${unit}`;
	const rangeStr = `${alert.target.min}–${alert.target.max}${unit}`;
	return {
		title: `${label} ${direction.charAt(0).toUpperCase() + direction.slice(1)}`,
		description: `${label} too ${direction} at ${valueStr} (target ${rangeStr})`,
	};
}

export const climateConfig = $state<ClimateConfig>({ ...DEFAULT_CLIMATE_CONFIG });
export const climateAlerts = $state<ClimateAlert[]>([]);
export const systemEvents = $state<SystemEvent[]>([]);

let lastSeenEventTimestamp = $state(Date.now());
let lastSeenAlertTimestamp = $state(Date.now());

export function markEventsSeen(): void {
	lastSeenEventTimestamp = Date.now();
}

export function markAlertsSeen(): void {
	lastSeenAlertTimestamp = Date.now();
}

export function getUnseenEventCount(): number {
	return systemEvents.filter((e) => e.timestamp.getTime() > lastSeenEventTimestamp).length;
}

export function getUnseenAlertCount(): number {
	return climateAlerts.filter((a) => a.timestamp.getTime() > lastSeenAlertTimestamp).length;
}

const MAX_EVENTS = 100;

function pushEvent(event: SystemEvent): void {
	systemEvents.unshift(event);
	if (systemEvents.length > MAX_EVENTS) {
		systemEvents.length = MAX_EVENTS;
	}
}

let serverStatus = $state<ClimateStatus | null>(null);

let lastDayState: boolean | null = null;

const HYSTERESIS_OFFSET = 5;

const SENSOR_TYPE_TO_TARGET_KEY: Partial<Record<SensorType, "temp" | "humidity" | "vpd" | "co2">> = {
	temperature: "temp",
	humidity: "humidity",
	vpd: "vpd",
	co2: "co2",
};

export const WARNING_MARGIN: Record<"temp" | "humidity" | "vpd" | "co2", number> = {
	temp: 2,
	humidity: 10,
	vpd: 0.2,
	co2: 200,
};

const _isDay = $derived.by((): boolean => {
	if (climateConfig.dayNightMode === "manual") {
		const schedule = climateConfig.manualSchedule;
		if (!schedule) return true;
		const now = new Date();
		const minutesSinceMidnight = now.getHours() * 60 + now.getMinutes();
		const dayMinutes = parseTimeToMinutes(schedule.dayStart);
		const nightMinutes = parseTimeToMinutes(schedule.nightStart);
		if (dayMinutes < nightMinutes) {
			return minutesSinceMidnight >= dayMinutes && minutesSinceMidnight < nightMinutes;
		}
		return minutesSinceMidnight >= dayMinutes || minutesSinceMidnight < nightMinutes;
	}

	const lightSensor = sensors.find((s) => s.type === "light");
	if (!lightSensor) return lastDayState ?? true;

	const reading = sensorReadings[lightSensor.id];
	if (!reading) return lastDayState ?? true;

	const threshold = climateConfig.lightThreshold;
	if (lastDayState === null) {
		lastDayState = reading.value >= threshold;
		return lastDayState;
	}

	if (lastDayState && reading.value < threshold - HYSTERESIS_OFFSET) {
		lastDayState = false;
	} else if (!lastDayState && reading.value > threshold + HYSTERESIS_OFFSET) {
		lastDayState = true;
	}

	return lastDayState;
});

const _currentTargets = $derived.by((): PhaseTargets => {
	return climateConfig.phases[climateConfig.activePhase];
});

const _sensorStatuses = $derived.by((): Record<string, SensorStatus> => {
	if (serverStatus?.sensorStatuses) return serverStatus.sensorStatuses;

	const statuses: Record<string, SensorStatus> = {};
	const targets = _currentTargets;
	const dayTime = _isDay;

	for (const sensor of sensors) {
		const targetKey = SENSOR_TYPE_TO_TARGET_KEY[sensor.type];
		if (!targetKey) continue;

		const reading = sensorReadings[sensor.id];
		if (!reading) continue;

		const target = targets[targetKey][dayTime ? "day" : "night"];
		const margin = WARNING_MARGIN[targetKey];
		const diff = Math.abs(reading.value - target);

		if (diff <= margin) {
			statuses[sensor.id] = "optimal";
		} else if (diff <= margin * 2) {
			statuses[sensor.id] = "warning";
		} else {
			statuses[sensor.id] = "critical";
		}
	}

	return statuses;
});

const _healthScore = $derived.by((): number => {
	if (serverStatus) return serverStatus.healthScore;

	const entries = Object.values(_sensorStatuses);
	if (entries.length === 0) return 100;

	const scores: Record<SensorStatus, number> = { optimal: 100, warning: 50, critical: 0 };
	const total = entries.reduce((sum, status) => sum + scores[status], 0);
	return Math.round(total / entries.length);
});

const _latestAlert = $derived<ClimateAlert | undefined>(
	serverStatus?.latestAlert ?? climateAlerts[0]
);

let _dliFromFirmware = $state(0);

const _dliTarget = $derived(_currentTargets.dli);

const _phaseDay = $derived.by((): number => {
	if (!climateConfig.phaseStartDate) return 1;
	const start = new Date(climateConfig.phaseStartDate);
	const now = new Date();
	const diffMs = now.getTime() - start.getTime();
	return Math.max(1, Math.floor(diffMs / (1000 * 60 * 60 * 24)) + 1);
});

export function getIsDay(): boolean {
	return _isDay;
}

export function getCurrentTargets(): PhaseTargets {
	return _currentTargets;
}

export function getHealthScore(): number {
	return _healthScore;
}

export function getLatestAlert(): ClimateAlert | undefined {
	return _latestAlert;
}

export function getDli(): number {
	return _dliFromFirmware;
}

export function getDliTarget(): number {
	return _dliTarget;
}

export function getPhaseDay(): number {
	return _phaseDay;
}

export function getSensorStatus(sensorId: string): SensorStatus | undefined {
	return _sensorStatuses[sensorId];
}

export function getSensorTarget(sensorType: SensorType): number | undefined {
	const targetKey = SENSOR_TYPE_TO_TARGET_KEY[sensorType];
	if (!targetKey) return undefined;
	return _currentTargets[targetKey][_isDay ? "day" : "night"];
}

export function setActivePhase(phase: ClimatePhase): void {
	climateConfig.activePhase = phase;
	climateConfig.phaseStartDate = new Date().toISOString();
	websocket.send("set_climate_phase", { phase, phaseStartDate: climateConfig.phaseStartDate });
}

export function updatePhaseTargets(phase: ClimatePhase, targets: PhaseTargets): void {
	climateConfig.phases[phase] = targets;
	websocket.send("set_climate_targets", { phase, targets });
}

export function setDayNightMode(mode: "auto" | "manual"): void {
	climateConfig.dayNightMode = mode;
	websocket.send("set_climate_daynight", { mode });
}

export function setManualSchedule(dayStart: string, nightStart: string): void {
	climateConfig.manualSchedule = { dayStart, nightStart };
	websocket.send("set_climate_schedule", { dayStart, nightStart });
}

export function setLightThreshold(threshold: number): void {
	climateConfig.lightThreshold = threshold;
	websocket.send("set_climate_light_threshold", { threshold });
}

export function resetPhaseTargets(phase: ClimatePhase): void {
	const defaults = DEFAULT_PHASE_TARGETS[phase];
	climateConfig.phases[phase] = { ...defaults };
	websocket.send("set_climate_targets", { phase, targets: defaults });
}

export function initClimateWebSocket(): void {
	websocket.on("climate_config", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const msg = data as Record<string, unknown>;

		if (typeof msg.activePhase === "string") {
			climateConfig.activePhase = msg.activePhase as ClimatePhase;
		}
		if (msg.phases && typeof msg.phases === "object") {
			Object.assign(climateConfig.phases, msg.phases);
		}
		if (typeof msg.dayNightMode === "string") {
			climateConfig.dayNightMode = msg.dayNightMode as "auto" | "manual";
		}
		if (msg.manualSchedule && typeof msg.manualSchedule === "object") {
			const sched = msg.manualSchedule as Record<string, unknown>;
			if (typeof sched.dayStart === "string" && typeof sched.nightStart === "string") {
				climateConfig.manualSchedule = {
					dayStart: sched.dayStart,
					nightStart: sched.nightStart,
				};
			}
		}
		if (typeof msg.lightThreshold === "number") {
			climateConfig.lightThreshold = msg.lightThreshold;
		}
		if (typeof msg.phaseStartDate === "string") {
			climateConfig.phaseStartDate = msg.phaseStartDate;
		}
	});

	websocket.on("climate_status", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const msg = data as Record<string, unknown>;

		const status: ClimateStatus = {
			isDay: typeof msg.isDay === "boolean" ? msg.isDay : _isDay,
			healthScore: typeof msg.healthScore === "number" ? msg.healthScore : 100,
			sensorStatuses: (msg.sensorStatuses as Record<string, SensorStatus>) ?? {},
			currentTargets: (msg.currentTargets as PhaseTargets) ?? _currentTargets,
		};

		if (msg.latestAlert && typeof msg.latestAlert === "object") {
			const alert = msg.latestAlert as Record<string, unknown>;
			status.latestAlert = {
				id: String(alert.id ?? ""),
				sensorId: String(alert.sensorId ?? ""),
				sensorType: String(alert.sensorType ?? "temperature") as SensorType,
				value: Number(alert.value ?? 0),
				target: (alert.target as { min: number; max: number }) ?? { min: 0, max: 0 },
				severity: (alert.severity as "warning" | "critical") ?? "warning",
				timestamp: new Date(String(alert.timestamp ?? Date.now())),
			};
		}

		serverStatus = status;
	});

	websocket.on("alert", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const msg = data as Record<string, unknown>;

		const value = Number(msg.value ?? 0);
		if (!Number.isFinite(value)) return;

		const alert: ClimateAlert = {
			id: String(msg.id ?? crypto.randomUUID()),
			sensorId: String(msg.sensorId ?? ""),
			sensorType: String(msg.sensorType ?? "temperature") as SensorType,
			value,
			target: (msg.target as { min: number; max: number }) ?? { min: 0, max: 0 },
			severity: (msg.severity as "warning" | "critical") ?? "warning",
			timestamp: new Date(String(msg.timestamp ?? Date.now())),
		};

		climateAlerts.unshift(alert);
		if (climateAlerts.length > 100) {
			climateAlerts.length = 100;
		}

		const { title, description } = formatAlert(alert);

		pushEvent({
			id: alert.id,
			type: "alert",
			title,
			description,
			severity: alert.severity,
			timestamp: alert.timestamp,
		});
	});

	websocket.on("event", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const msg = data as Record<string, unknown>;

		const eventType = String(msg.eventType ?? msg.type ?? "system") as SystemEventType;
		const severity = String(msg.severity ?? "info") as "info" | "warning" | "critical";
		const timestamp = typeof msg.timestamp === "number"
			? new Date(msg.timestamp * 1000) : new Date();

		pushEvent({
			id: String(msg.id ?? crypto.randomUUID()),
			type: eventType,
			title: String(msg.title ?? ""),
			description: String(msg.description ?? ""),
			severity,
			timestamp,
		});

		if (eventType === "alert") {
			climateAlerts.unshift({
				id: String(msg.id ?? crypto.randomUUID()),
				sensorId: "",
				sensorType: "temperature",
				value: 0,
				target: { min: 0, max: 0 },
				severity: severity === "critical" ? "critical" : "warning",
				timestamp,
			});
			if (climateAlerts.length > 100) {
				climateAlerts.length = 100;
			}
		}
	});

	websocket.on("events", (data: unknown) => {
		if (!Array.isArray(data)) return;
		for (const item of data) {
			if (!item || typeof item !== "object") continue;
			const msg = item as Record<string, unknown>;
			const eventType = String(msg.type ?? "system") as SystemEventType;
			const severity = String(msg.severity ?? "info") as "info" | "warning" | "critical";
			const timestamp = typeof msg.timestamp === "number"
				? new Date(msg.timestamp * 1000) : new Date();

			systemEvents.push({
				id: String(msg.id ?? crypto.randomUUID()),
				type: eventType,
				title: String(msg.title ?? ""),
				description: String(msg.description ?? ""),
				severity,
				timestamp,
			});
		}
		systemEvents.sort((a, b) => b.timestamp.getTime() - a.timestamp.getTime());
		if (systemEvents.length > MAX_EVENTS) {
			systemEvents.length = MAX_EVENTS;
		}
	});

	websocket.on("dli", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const msg = data as Record<string, unknown>;
		if (typeof msg.dli === "number") {
			_dliFromFirmware = msg.dli;
		}
	});
}

function parseTimeToMinutes(time: string): number {
	const match = /^(\d{1,2}):(\d{2})$/.exec(time);
	if (!match) return 0;
	return Number(match[1]) * 60 + Number(match[2]);
}
