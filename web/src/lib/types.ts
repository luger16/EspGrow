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
	leafTempOffset?: number;
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
	isOnline?: boolean;
	controlMode: DeviceControlMode;
	hasEnergyMonitoring?: boolean;
}

export type DeviceMode = "off" | "on" | "auto" | "cycle" | "schedule";

export interface AutoTrigger {
	sensorType: string;
	dayThreshold: number;
	nightThreshold: number;
	hysteresis: number;
	triggerAbove: boolean;
}

export interface CycleConfig {
	onDurationSec: number;
	offDurationSec: number;
	dayOnly: boolean;
}

export interface ScheduleConfig {
	startTime: string;
	endTime: string;
}

export interface DeviceModeConfig {
	deviceId: string;
	mode: DeviceMode;
	triggers: AutoTrigger[];
	cycle: CycleConfig;
	schedule: ScheduleConfig;
}

export interface DayNightConfig {
	dayStartTime: string;
	nightStartTime: string;
	lightThreshold: number;
	useSchedule: boolean;
	isDaytime?: boolean;
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

// Energy tracking

export interface DeviceEnergy {
	deviceId: string;
	deviceName: string;
	watts: number;
	kWh: number;
	resetTimestamp: Date;
}

export interface SpectralData {
	channels: number[];
	timestamp: Date;
}
