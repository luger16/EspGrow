export type SensorType = "temperature" | "humidity" | "co2" | "light" | "vpd";

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
	sensorId: string;
	operator: ComparisonOperator;
	threshold: number;
	thresholdOff?: number;
	useHysteresis?: boolean;
	minRunTimeMs?: number;
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
