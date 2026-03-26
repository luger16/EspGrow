import type { DeviceModeConfig, DayNightConfig } from "$lib/types";
import { websocket } from "./websocket.svelte";

export const deviceModes = $state<DeviceModeConfig[]>([]);
export const dayNightConfig = $state<DayNightConfig>({
	dayStartTime: "06:00",
	nightStartTime: "22:00",
	lightThreshold: 50,
	useSchedule: false,
});

export function setDeviceMode(config: DeviceModeConfig): void {
	websocket.send("set_device_mode", {
		deviceId: config.deviceId,
		mode: config.mode,
		triggers: config.triggers,
		cycle: config.cycle,
		schedule: config.schedule,
	});
}

export function deleteDeviceMode(deviceId: string): void {
	websocket.send("delete_device_mode", { deviceId });
}

export function setDayNightConfig(config: Omit<DayNightConfig, "isDaytime">): void {
	websocket.send("set_daynight_config", {
		useSchedule: config.useSchedule,
		dayStartTime: config.dayStartTime,
		nightStartTime: config.nightStartTime,
		lightThreshold: config.lightThreshold,
	});
}

export function getDeviceMode(deviceId: string): DeviceModeConfig | undefined {
	return deviceModes.find((m) => m.deviceId === deviceId);
}

export function initDeviceModesWebSocket(): void {
	websocket.on("device_modes", (data: unknown) => {
		if (!Array.isArray(data)) return;
		const items = data.filter(
			(item): item is DeviceModeConfig =>
				item !== null &&
				typeof item === "object" &&
				typeof (item as Record<string, unknown>).deviceId === "string" &&
				typeof (item as Record<string, unknown>).mode === "string",
		);
		deviceModes.length = 0;
		deviceModes.push(...items);
	});

	websocket.on("daynight_config", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const cfg = data as Record<string, unknown>;
		if (typeof cfg.dayStartTime === "string") dayNightConfig.dayStartTime = cfg.dayStartTime;
		if (typeof cfg.nightStartTime === "string") dayNightConfig.nightStartTime = cfg.nightStartTime;
		if (typeof cfg.lightThreshold === "number") dayNightConfig.lightThreshold = cfg.lightThreshold;
		if (typeof cfg.useSchedule === "boolean") dayNightConfig.useSchedule = cfg.useSchedule;
		if (typeof cfg.isDaytime === "boolean") dayNightConfig.isDaytime = cfg.isDaytime;
	});

	websocket.send("get_device_modes");
	websocket.send("get_daynight_config");
}
