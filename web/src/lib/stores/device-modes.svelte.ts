import type { DeviceModeConfig } from "$lib/types";
import { websocket } from "./websocket.svelte";
import { localToUtc, utcToLocal } from "$lib/utils";

export const deviceModes = $state<DeviceModeConfig[]>([]);

export function setDeviceMode(config: DeviceModeConfig): void {
	websocket.send("set_device_mode", {
		deviceId: config.deviceId,
		mode: config.mode,
		triggers: config.triggers ?? [],
		cycle: config.cycle,
		schedule: config.schedule
			? {
					startTime: localToUtc(config.schedule.startTime),
					endTime: localToUtc(config.schedule.endTime),
				}
			: undefined,
	});
}

export function deleteDeviceMode(deviceId: string): void {
	websocket.send("delete_device_mode", { deviceId });
}

export function getDeviceMode(deviceId: string): DeviceModeConfig | undefined {
	return deviceModes.find((m) => m.deviceId === deviceId);
}

export function initDeviceModesWebSocket(): void {
	websocket.on("device_modes", (data: unknown) => {
		if (!Array.isArray(data)) return;
		const items = data
			.filter(
				(item): item is DeviceModeConfig =>
					item !== null &&
					typeof item === "object" &&
					typeof (item as Record<string, unknown>).deviceId === "string" &&
					typeof (item as Record<string, unknown>).mode === "string"
			)
			.map((item) => ({
				...item,
				schedule: item.schedule
					? {
							startTime: utcToLocal(item.schedule.startTime),
							endTime: utcToLocal(item.schedule.endTime),
						}
					: item.schedule,
			}));
		deviceModes.length = 0;
		deviceModes.push(...items);
	});
}
