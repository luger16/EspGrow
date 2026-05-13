import type {
	ClimateConfig,
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

export const climateConfig = $state<ClimateConfig>({ ...DEFAULT_CLIMATE_CONFIG });
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
	return systemEvents.filter(
		(e) => e.type === "alert" && e.timestamp.getTime() > lastSeenAlertTimestamp
	).length;
}

export function clearEvents(): void {
	systemEvents.length = 0;
	websocket.send("clear_events");
}

const MAX_EVENTS = 100;

function pushEvent(event: SystemEvent): void {
	systemEvents.unshift(event);
	if (systemEvents.length > MAX_EVENTS) {
		systemEvents.length = MAX_EVENTS;
	}
}

let lastDayState: boolean | null = null;

const LIGHT_THRESHOLD = 10;
const HYSTERESIS_OFFSET = 5;

const SENSOR_TYPE_TO_TARGET_KEY: Partial<Record<SensorType, "temp" | "humidity" | "vpd" | "co2">> =
	{
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
	const lightSensor = sensors.find((s) => s.type === "light");
	if (!lightSensor) return lastDayState ?? true;

	const reading = sensorReadings[lightSensor.id];
	if (!reading) return lastDayState ?? true;

	if (lastDayState === null) {
		lastDayState = reading.value >= LIGHT_THRESHOLD;
		return lastDayState;
	}

	if (lastDayState && reading.value < LIGHT_THRESHOLD - HYSTERESIS_OFFSET) {
		lastDayState = false;
	} else if (!lastDayState && reading.value > LIGHT_THRESHOLD + HYSTERESIS_OFFSET) {
		lastDayState = true;
	}

	return lastDayState;
});

const _currentTargets = $derived.by((): PhaseTargets => {
	return climateConfig.phases[climateConfig.activePhase];
});

const _sensorStatuses = $derived.by((): Record<string, SensorStatus> => {
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
	const entries = Object.values(_sensorStatuses);
	if (entries.length === 0) return 100;

	const scores: Record<SensorStatus, number> = { optimal: 100, warning: 50, critical: 0 };
	const total = entries.reduce((sum, status) => sum + scores[status], 0);
	return Math.round(total / entries.length);
});

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
		if (typeof msg.phaseStartDate === "string") {
			climateConfig.phaseStartDate = msg.phaseStartDate;
		}
	});

	websocket.on("event", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const msg = data as Record<string, unknown>;

		const eventType = String(msg.eventType ?? msg.type ?? "system") as SystemEventType;
		const severity = String(msg.severity ?? "info") as "info" | "warning" | "critical";
		const timestamp =
			typeof msg.timestamp === "number" ? new Date(msg.timestamp * 1000) : new Date();

		pushEvent({
			id: String(msg.id ?? crypto.randomUUID()),
			type: eventType,
			title: String(msg.title ?? ""),
			description: String(msg.description ?? ""),
			severity,
			timestamp,
		});
	});

	websocket.on("events", (data: unknown) => {
		if (!Array.isArray(data)) return;
		const existingIds = new Set(systemEvents.map((e) => e.id));
		for (const item of data) {
			if (!item || typeof item !== "object") continue;
			const msg = item as Record<string, unknown>;
			const id = String(msg.id ?? crypto.randomUUID());
			if (existingIds.has(id)) continue;
			const eventType = String(msg.type ?? "system") as SystemEventType;
			const severity = String(msg.severity ?? "info") as "info" | "warning" | "critical";
			const timestamp =
				typeof msg.timestamp === "number" ? new Date(msg.timestamp * 1000) : new Date();

			systemEvents.push({
				id,
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
