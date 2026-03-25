export type SensorType = "temperature" | "humidity" | "co2" | "light" | "vpd" | "dewpoint";

export interface Sensor {
	id: string;
	name: string;
	type: SensorType;
	unit: string;
	hardwareType: "sht3x" | "sht4x" | "scd4x" | "as7341" | "calculated";
	address?: string;
	tempSourceId?: string;
	humSourceId?: string;
}

export interface SensorReading {
	sensorId: string;
	value: number;
	timestamp: Date;
}

export interface HistoricalReading {
	date: Date;
	value: number;
}

export type DeviceType = "fan" | "light" | "heater" | "pump" | "humidifier" | "dehumidifier";
export type DeviceControlMethod = "shelly_gen1" | "shelly_gen2" | "tasmota";
export type DeviceControlMode = "manual" | "automatic";

export interface Device {
	id: string;
	name: string;
	type: DeviceType;
	controlMethod: DeviceControlMethod;
	ipAddress?: string;
	isOn: boolean;
	controlMode: DeviceControlMode;
}

export type ComparisonOperator = ">" | ">=" | "<" | "<=" | "=";

export interface AutomationRule {
	id: string;
	name: string;
	enabled: boolean;
	type: "sensor" | "schedule";
	sensorId?: string;
	operator?: ComparisonOperator;
	threshold?: number;
	thresholdOff?: number;
	useHysteresis?: boolean;
	minRunTimeMs?: number;
	onTime?: string;
	offTime?: string;
	deviceId: string;
	action: "turn_on" | "turn_off";
}

export interface SystemInfo {
	uptime: number;
	freeHeap: number;
	chipModel: string;
	wifiRssi: number;
	ipAddress: string;
	firmwareVersion: string;
}

// Climate supervision

export type ClimatePhase = "seedling" | "veg" | "flower" | "dry";

export type SensorStatus = "optimal" | "warning" | "critical";

export interface PhaseTargets {
	temp: { day: number; night: number };
	humidity: { day: number; night: number };
	vpd: { day: number; night: number };
	co2: { day: number; night: number };
}

export interface ClimateConfig {
	activePhase: ClimatePhase;
	phases: Record<ClimatePhase, PhaseTargets>;
	dayNightMode: "auto" | "manual";
	manualSchedule?: { dayStart: string; nightStart: string };
	lightThreshold: number;
}

export interface ClimateAlert {
	id: string;
	sensorId: string;
	sensorType: SensorType;
	value: number;
	target: { min: number; max: number };
	severity: "warning" | "critical";
	timestamp: Date;
}

export interface ClimateStatus {
	isDay: boolean;
	healthScore: number;
	sensorStatuses: Record<string, SensorStatus>;
	currentTargets: PhaseTargets;
	latestAlert?: ClimateAlert;
}

// System events (unified event history)

export type SystemEventType = "alert" | "automation" | "device" | "system";

export interface SystemEvent {
	id: string;
	type: SystemEventType;
	title: string;
	description: string;
	severity?: "info" | "warning" | "critical";
	timestamp: Date;
}
