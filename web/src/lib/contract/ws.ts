/**
 * WebSocket protocol contract — single source of truth for both web and firmware.
 *
 * Every message exchanged between the SvelteKit UI and the ESP32 firmware MUST be
 * declared here. The TypeScript types consumed across the web app are derived from
 * these schemas via `v.InferOutput`. The firmware pulls a generated `contract.h`
 * (see `web/scripts/gen-contract.mjs`) that mirrors the message-type tags.
 *
 * Envelope: every frame is `{ "type": <tag>, "data"?: <payload> }`. Payload schemas
 * use `v.strictObject` to reject unknown keys and surface drift instantly during
 * development.
 */

import * as v from "valibot";

// -----------------------------------------------------------------------------
// Shared sub-schemas
// -----------------------------------------------------------------------------

export const SensorTypeSchema = v.picklist([
	"temperature",
	"humidity",
	"co2",
	"light",
	"vpd",
	"dewpoint",
]);

export const HardwareTypeSchema = v.picklist(["sht3x", "sht4x", "scd4x", "as7341", "calculated"]);

export const DeviceTypeSchema = v.picklist([
	"fan",
	"light",
	"heater",
	"pump",
	"humidifier",
	"dehumidifier",
]);

export const DeviceControlMethodSchema = v.picklist(["shelly_gen1", "shelly_gen2", "tasmota"]);
export const DeviceControlModeSchema = v.picklist(["manual", "automatic"]);
export const DeviceModeSchema = v.picklist(["off", "on", "auto", "cycle", "schedule"]);
export const ClimatePhaseSchema = v.picklist(["seedling", "veg", "flower", "dry"]);
export const SystemEventTypeSchema = v.picklist(["alert", "automation", "device", "system"]);
export const SeveritySchema = v.picklist(["info", "warning", "critical"]);
export const HistoryRangeSchema = v.picklist(["6h", "24h", "7d"]);

export const SensorSchema = v.strictObject({
	id: v.string(),
	name: v.string(),
	type: SensorTypeSchema,
	unit: v.string(),
	hardwareType: HardwareTypeSchema,
	address: v.optional(v.string()),
	tempSourceId: v.optional(v.string()),
	humSourceId: v.optional(v.string()),
	leafTempOffset: v.optional(v.number()),
});

export const SensorReadingPayloadSchema = v.strictObject({
	id: v.string(),
	type: v.optional(v.string()),
	value: v.number(),
	timestamp: v.optional(v.number()),
	channels: v.optional(v.array(v.number())),
});

export const DeviceSchema = v.strictObject({
	id: v.string(),
	name: v.string(),
	type: DeviceTypeSchema,
	controlMethod: DeviceControlMethodSchema,
	ipAddress: v.optional(v.string()),
	isOn: v.boolean(),
	isOnline: v.optional(v.boolean()),
	controlMode: DeviceControlModeSchema,
	hasEnergyMonitoring: v.optional(v.boolean()),
	timestamp: v.optional(v.number()),
});

export const AutoTriggerSchema = v.strictObject({
	sensorId: v.string(),
	sensorType: v.optional(v.string()),
	dayThreshold: v.number(),
	nightThreshold: v.number(),
	deadzone: v.number(),
	triggerAbove: v.boolean(),
});

export const CycleConfigSchema = v.strictObject({
	onDurationSec: v.number(),
	offDurationSec: v.number(),
	dayOnly: v.boolean(),
});

export const ScheduleConfigSchema = v.strictObject({
	startTime: v.string(),
	endTime: v.string(),
});

export const DeviceModeConfigSchema = v.strictObject({
	deviceId: v.string(),
	mode: DeviceModeSchema,
	triggers: v.optional(v.array(AutoTriggerSchema)),
	cycle: v.optional(CycleConfigSchema),
	schedule: v.optional(ScheduleConfigSchema),
});

export const DayNightTargetSchema = v.strictObject({
	day: v.number(),
	night: v.number(),
});

export const PhaseTargetsSchema = v.strictObject({
	temp: DayNightTargetSchema,
	humidity: DayNightTargetSchema,
	vpd: DayNightTargetSchema,
	co2: DayNightTargetSchema,
	dli: v.number(),
});

export const ClimateConfigPayloadSchema = v.strictObject({
	activePhase: ClimatePhaseSchema,
	phases: v.strictObject({
		seedling: PhaseTargetsSchema,
		veg: PhaseTargetsSchema,
		flower: PhaseTargetsSchema,
		dry: PhaseTargetsSchema,
	}),
	phaseStartDate: v.optional(v.string()),
});

export const EventEntrySchema = v.strictObject({
	id: v.optional(v.string()),
	type: v.optional(SystemEventTypeSchema),
	eventType: v.optional(SystemEventTypeSchema),
	title: v.optional(v.string()),
	description: v.optional(v.string()),
	severity: v.optional(SeveritySchema),
	timestamp: v.optional(v.number()),
});

export const DeviceEnergySchema = v.strictObject({
	deviceId: v.string(),
	deviceName: v.optional(v.string()),
	watts: v.number(),
	kWh: v.number(),
	resetTimestamp: v.optional(v.number()),
});

// -----------------------------------------------------------------------------
// Envelope helper
// -----------------------------------------------------------------------------

function frame<TTag extends string, TData>(
	tag: TTag,
	data: v.BaseSchema<TData, TData, v.BaseIssue<unknown>>
) {
	return v.strictObject({
		type: v.literal(tag),
		data,
	});
}

function frameNoData<TTag extends string>(tag: TTag) {
	return v.strictObject({
		type: v.literal(tag),
		data: v.optional(v.strictObject({})),
	});
}

// -----------------------------------------------------------------------------
// Server -> Client messages
// -----------------------------------------------------------------------------

export const PongMessage = frame("pong", v.strictObject({ timestamp: v.number() }));

export const SensorsMessage = frame("sensors", v.array(SensorReadingPayloadSchema));

export const SensorConfigMessage = frame("sensor_config", v.array(SensorSchema));

export const DevicesMessage = frame("devices", v.array(DeviceSchema));

export const DeviceModesMessage = frame("device_modes", v.array(DeviceModeConfigSchema));

export const ClimateConfigMessage = frame("climate_config", ClimateConfigPayloadSchema);

export const EventsMessage = frame("events", v.array(EventEntrySchema));

export const EventMessage = frame("event", EventEntrySchema);

export const EnergyMessage = frame("energy", v.array(DeviceEnergySchema));

export const DliMessage = frame("dli", v.strictObject({ dli: v.number() }));

export const HistoryMessage = frame(
	"history",
	v.strictObject({
		sensorId: v.string(),
		range: HistoryRangeSchema,
		pointSize: v.number(),
		count: v.number(),
		payload: v.string(),
	})
);

export const PpfdCalibrationMessage = frame(
	"ppfd_calibration",
	v.strictObject({
		factor: v.optional(v.number()),
		success: v.optional(v.boolean()),
		error: v.optional(v.string()),
	})
);

export const SystemInfoMessage = frame(
	"system_info",
	v.strictObject({
		uptime: v.number(),
		freeHeap: v.number(),
		chipModel: v.string(),
		wifiRssi: v.number(),
		ipAddress: v.string(),
		firmwareVersion: v.string(),
	})
);

export const ClearHistoryAck = frame("clear_history", v.strictObject({ success: v.boolean() }));

export const RestartAck = frame("restart", v.strictObject({ success: v.boolean() }));

export const OtaStatusMessage = frame(
	"ota_status",
	v.strictObject({
		state: v.string(),
		progress: v.optional(v.number()),
		message: v.optional(v.string()),
		error: v.optional(v.string()),
	})
);

export const ServerToClientMessage = v.variant("type", [
	PongMessage,
	SensorsMessage,
	SensorConfigMessage,
	DevicesMessage,
	DeviceModesMessage,
	ClimateConfigMessage,
	EventsMessage,
	EventMessage,
	EnergyMessage,
	DliMessage,
	HistoryMessage,
	PpfdCalibrationMessage,
	SystemInfoMessage,
	ClearHistoryAck,
	RestartAck,
	OtaStatusMessage,
]);

// -----------------------------------------------------------------------------
// Client -> Server messages
// -----------------------------------------------------------------------------

export const PingRequest = frameNoData("ping");
export const GetInitRequest = frameNoData("get_init");
export const GetDeviceModesRequest = frameNoData("get_device_modes");
export const GetDevicesRequest = frameNoData("get_devices");
export const GetSensorsRequest = frameNoData("get_sensors");
export const GetPpfdCalibrationRequest = frameNoData("get_ppfd_calibration");
export const GetEnergyRequest = frameNoData("get_energy");
export const GetDliRequest = frameNoData("get_dli");
export const GetClimateConfigRequest = frameNoData("get_climate_config");
export const GetEventsRequest = frameNoData("get_events");
export const ClearEventsRequest = frameNoData("clear_events");
export const GetSystemInfoRequest = frameNoData("get_system_info");
export const ResetDliRequest = frameNoData("reset_dli");
export const ResetPpfdCalibrationRequest = frameNoData("reset_ppfd_calibration");
export const ClearHistoryRequest = frameNoData("clear_history");
export const RestartRequest = frameNoData("restart");

export const GetHistoryRequest = frame(
	"get_history",
	v.strictObject({ sensorId: v.string(), range: HistoryRangeSchema })
);

export const DeviceControlRequest = frame(
	"device_control",
	v.strictObject({
		method: v.string(),
		target: v.string(),
		on: v.boolean(),
	})
);

export const SetDeviceModeRequest = frame(
	"set_device_mode",
	v.strictObject({
		deviceId: v.string(),
		mode: DeviceModeSchema,
		triggers: v.optional(v.array(AutoTriggerSchema)),
		cycle: v.optional(CycleConfigSchema),
		schedule: v.optional(ScheduleConfigSchema),
	})
);

export const DeleteDeviceModeRequest = frame(
	"delete_device_mode",
	v.strictObject({ deviceId: v.string() })
);

export const AddDeviceRequest = frame(
	"add_device",
	v.strictObject({
		id: v.string(),
		name: v.string(),
		deviceType: DeviceTypeSchema,
		controlMethod: DeviceControlMethodSchema,
		ipAddress: v.optional(v.string()),
		controlMode: DeviceControlModeSchema,
		hasEnergyMonitoring: v.optional(v.boolean()),
	})
);

export const UpdateDeviceRequest = frame(
	"update_device",
	v.strictObject({
		id: v.string(),
		name: v.optional(v.string()),
		deviceType: v.optional(DeviceTypeSchema),
		controlMethod: v.optional(DeviceControlMethodSchema),
		ipAddress: v.optional(v.string()),
		controlMode: v.optional(DeviceControlModeSchema),
		hasEnergyMonitoring: v.optional(v.boolean()),
	})
);

export const RemoveDeviceRequest = frame("remove_device", v.strictObject({ id: v.string() }));

export const AddSensorRequest = frame(
	"add_sensor",
	v.strictObject({
		id: v.string(),
		name: v.string(),
		sensorType: SensorTypeSchema,
		unit: v.string(),
		hardwareType: HardwareTypeSchema,
		address: v.optional(v.string()),
		tempSourceId: v.optional(v.string()),
		humSourceId: v.optional(v.string()),
		leafTempOffset: v.optional(v.number()),
	})
);

export const UpdateSensorRequest = frame(
	"update_sensor",
	v.strictObject({
		id: v.string(),
		name: v.optional(v.string()),
		sensorType: v.optional(SensorTypeSchema),
		unit: v.optional(v.string()),
		hardwareType: v.optional(HardwareTypeSchema),
		address: v.optional(v.string()),
		tempSourceId: v.optional(v.string()),
		humSourceId: v.optional(v.string()),
		leafTempOffset: v.optional(v.number()),
	})
);

export const RemoveSensorRequest = frame("remove_sensor", v.strictObject({ id: v.string() }));

export const CalibratePpfdRequest = frame(
	"calibrate_ppfd",
	v.strictObject({ knownPpfd: v.number() })
);

export const ResetEnergyRequest = frame(
	"reset_energy",
	v.strictObject({ deviceId: v.optional(v.string()) })
);

export const SetClimatePhaseRequest = frame(
	"set_climate_phase",
	v.strictObject({
		phase: ClimatePhaseSchema,
		phaseStartDate: v.optional(v.string()),
	})
);

export const SetClimateTargetsRequest = frame(
	"set_climate_targets",
	v.strictObject({
		phase: ClimatePhaseSchema,
		targets: PhaseTargetsSchema,
	})
);

export const ResetClimateTargetsRequest = frame(
	"reset_climate_targets",
	v.strictObject({ phase: ClimatePhaseSchema })
);

export const ClientToServerMessage = v.variant("type", [
	PingRequest,
	GetInitRequest,
	GetDeviceModesRequest,
	GetDevicesRequest,
	GetSensorsRequest,
	GetPpfdCalibrationRequest,
	GetEnergyRequest,
	GetDliRequest,
	GetClimateConfigRequest,
	GetEventsRequest,
	ClearEventsRequest,
	GetSystemInfoRequest,
	ResetDliRequest,
	ResetPpfdCalibrationRequest,
	ClearHistoryRequest,
	RestartRequest,
	GetHistoryRequest,
	DeviceControlRequest,
	SetDeviceModeRequest,
	DeleteDeviceModeRequest,
	AddDeviceRequest,
	UpdateDeviceRequest,
	RemoveDeviceRequest,
	AddSensorRequest,
	UpdateSensorRequest,
	RemoveSensorRequest,
	CalibratePpfdRequest,
	ResetEnergyRequest,
	SetClimatePhaseRequest,
	SetClimateTargetsRequest,
	ResetClimateTargetsRequest,
]);

// -----------------------------------------------------------------------------
// Codegen manifest — consumed by web/scripts/gen-contract.mjs
//
// This list is the authoritative, ordered registry of every WS message tag.
// The codegen script reads it to emit `firmware/src/contract.h`.
// -----------------------------------------------------------------------------

export const WS_MESSAGE_TAGS = {
	serverToClient: [
		"pong",
		"sensors",
		"sensor_config",
		"devices",
		"device_modes",
		"climate_config",
		"events",
		"event",
		"energy",
		"dli",
		"history",
		"ppfd_calibration",
		"system_info",
		"clear_history",
		"restart",
		"ota_status",
	],
	clientToServer: [
		"ping",
		"get_init",
		"get_device_modes",
		"get_devices",
		"get_sensors",
		"get_ppfd_calibration",
		"get_energy",
		"get_dli",
		"get_climate_config",
		"get_events",
		"clear_events",
		"get_system_info",
		"reset_dli",
		"reset_ppfd_calibration",
		"clear_history",
		"restart",
		"get_history",
		"device_control",
		"set_device_mode",
		"delete_device_mode",
		"add_device",
		"update_device",
		"remove_device",
		"add_sensor",
		"update_sensor",
		"remove_sensor",
		"calibrate_ppfd",
		"reset_energy",
		"set_climate_phase",
		"set_climate_targets",
		"reset_climate_targets",
	],
} as const;

// -----------------------------------------------------------------------------
// Inferred TypeScript types
// -----------------------------------------------------------------------------

export type ServerToClient = v.InferOutput<typeof ServerToClientMessage>;
export type ClientToServer = v.InferOutput<typeof ClientToServerMessage>;

export type ServerMessageType = ServerToClient["type"];
export type ClientMessageType = ClientToServer["type"];

export type Sensor = v.InferOutput<typeof SensorSchema>;
export type Device = v.InferOutput<typeof DeviceSchema>;
export type DeviceModeConfig = v.InferOutput<typeof DeviceModeConfigSchema>;
export type DeviceMode = v.InferOutput<typeof DeviceModeSchema>;
export type DeviceType = v.InferOutput<typeof DeviceTypeSchema>;
export type DeviceControlMethod = v.InferOutput<typeof DeviceControlMethodSchema>;
export type DeviceControlMode = v.InferOutput<typeof DeviceControlModeSchema>;
export type SensorType = v.InferOutput<typeof SensorTypeSchema>;
export type ClimatePhase = v.InferOutput<typeof ClimatePhaseSchema>;
export type ClimateConfig = v.InferOutput<typeof ClimateConfigPayloadSchema>;
export type PhaseTargets = v.InferOutput<typeof PhaseTargetsSchema>;
export type AutoTrigger = v.InferOutput<typeof AutoTriggerSchema>;
export type CycleConfig = v.InferOutput<typeof CycleConfigSchema>;
export type ScheduleConfig = v.InferOutput<typeof ScheduleConfigSchema>;
export type SystemEventType = v.InferOutput<typeof SystemEventTypeSchema>;
export type DeviceEnergy = v.InferOutput<typeof DeviceEnergySchema>;
