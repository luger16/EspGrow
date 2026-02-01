export type SensorType = "temperature" | "humidity" | "co2" | "light" | "soil_moisture";

export interface Sensor {
	id: string;
	name: string;
	type: SensorType;
	unit: string;
	hardwareType: "sht41" | "scd40" | "as7341" | "soil_capacitive";
	address?: string;
}

export interface SensorReading {
	sensorId: string;
	value: number;
	timestamp: Date;
}

export type DeviceType = "fan" | "light" | "heater" | "pump" | "humidifier";
export type DeviceControlMethod = "relay" | "shelly" | "tasmota";
export type DeviceControlMode = "manual" | "automatic";

export interface Device {
	id: string;
	name: string;
	type: DeviceType;
	controlMethod: DeviceControlMethod;
	gpioPin?: number;
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
	deviceId: string;
	action: "turn_on" | "turn_off";
}
