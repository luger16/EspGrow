import type * as v from "valibot";
import type {
	SensorTypeSchema,
	DeviceTypeSchema,
	DeviceModeSchema,
	ClimatePhaseSchema,
	SystemEventTypeSchema,
	DeviceControlMethodSchema,
	DeviceControlModeSchema,
	SensorSchema,
	DeviceModeConfigSchema,
	AutoTriggerSchema,
	ClimateConfigPayloadSchema,
	PhaseTargetsSchema,
	SystemInfoMessage,
} from "$lib/contract/ws";

// Wire types are derived from the WS contract — do not hand-edit; add new fields in `$lib/contract/ws.ts`.
export type SensorType = v.InferOutput<typeof SensorTypeSchema>;
export type DeviceType = v.InferOutput<typeof DeviceTypeSchema>;
export type DeviceControlMethod = v.InferOutput<typeof DeviceControlMethodSchema>;
export type DeviceControlMode = v.InferOutput<typeof DeviceControlModeSchema>;
export type DeviceMode = v.InferOutput<typeof DeviceModeSchema>;
export type ClimatePhase = v.InferOutput<typeof ClimatePhaseSchema>;
export type SystemEventType = v.InferOutput<typeof SystemEventTypeSchema>;
export type Sensor = v.InferOutput<typeof SensorSchema>;
export type DeviceModeConfig = v.InferOutput<typeof DeviceModeConfigSchema>;
export type AutoTrigger = v.InferOutput<typeof AutoTriggerSchema>;
export type ClimateConfig = v.InferOutput<typeof ClimateConfigPayloadSchema>;
export type PhaseTargets = v.InferOutput<typeof PhaseTargetsSchema>;
export type SystemInfo = v.InferOutput<typeof SystemInfoMessage>["data"];

export type SensorStatus = "optimal" | "warning" | "critical";

export interface SensorReading {
	sensorId: string;
	value: number;
	timestamp: Date;
}

export interface HistoricalReading {
	date: Date;
	value: number;
}

export interface SpectralData {
	channels: number[];
	timestamp: Date;
}

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
	timestamp?: Date;
}

export interface DeviceEnergy {
	deviceId: string;
	deviceName: string;
	watts: number;
	kWh: number;
	resetTimestamp: Date;
}

export interface SystemEvent {
	id: string;
	type: SystemEventType;
	title: string;
	description: string;
	severity?: "info" | "warning" | "critical";
	timestamp: Date;
}
