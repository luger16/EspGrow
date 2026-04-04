import type { DeviceEnergy } from "$lib/types";
import { websocket } from "./websocket.svelte";

export const deviceEnergies = $state<DeviceEnergy[]>([]);

const _totalWatts = $derived(deviceEnergies.reduce((sum, d) => sum + d.watts, 0));
const _totalKWh = $derived(
	Math.round(deviceEnergies.reduce((sum, d) => sum + d.kWh, 0) * 100) / 100,
);

export function getTotalWatts(): number {
	return _totalWatts;
}

export function getTotalKWh(): number {
	return _totalKWh;
}

export function resetEnergy(deviceId?: string): void {
	websocket.send("reset_energy", deviceId ? { deviceId } : {});
}

export function initEnergyWebSocket(): void {
	websocket.on("energy", (data: unknown) => {
		if (!Array.isArray(data)) return;

		const items = data.filter(
			(item): item is Record<string, unknown> =>
				item !== null && typeof item === "object" && typeof (item as Record<string, unknown>).deviceId === "string",
		);

		deviceEnergies.length = 0;
		for (const item of items) {
			deviceEnergies.push({
				deviceId: String(item.deviceId),
				deviceName: String(item.deviceName ?? ""),
				watts: Number(item.watts ?? 0),
				kWh: Number(item.kWh ?? 0),
				resetTimestamp: new Date(Number(item.resetTimestamp ?? 0) * 1000),
			});
		}
	});
}
