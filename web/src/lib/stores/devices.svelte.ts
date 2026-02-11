import type { Device } from "$lib/types";
import { websocket } from "./websocket.svelte";

export const devices = $state<Device[]>([]);
export const pendingDevices = $state<Set<string>>(new Set());
export const overriddenDevices = $state<Record<string, number>>({});

function getDeviceTarget(device: Device): string {
	return device.ipAddress ?? "";
}

export function toggleDevice(deviceId: string): void {
	const device = devices.find((d) => d.id === deviceId);
	if (!device) return;
	if (pendingDevices.has(deviceId)) return;

	pendingDevices.add(deviceId);
	const newState = !device.isOn;
	
	websocket.send("device_control", {
		method: device.controlMethod,
		target: getDeviceTarget(device),
		on: newState,
	});
}

export function addDevice(device: Device): void {
	websocket.send("add_device", {
		id: device.id,
		name: device.name,
		deviceType: device.type,
		controlMethod: device.controlMethod,
		ipAddress: device.ipAddress,
	});
}

export function removeDevice(deviceId: string): void {
	websocket.send("remove_device", { id: deviceId });
}

export function updateDevice(deviceId: string, updates: Partial<Omit<Device, "id">>): void {
	websocket.send("update_device", {
		id: deviceId,
		name: updates.name,
		deviceType: updates.type,
		controlMethod: updates.controlMethod,
		ipAddress: updates.ipAddress,
	});
}

export function initDeviceWebSocket(): void {
	websocket.on("devices", (data: unknown) => {
		const msg = data as { data: Device[] };
		if (Array.isArray(msg.data)) {
			devices.length = 0;
			msg.data.forEach((d) => {
				devices.push({
					...d,
					isOn: d.isOn ?? false,
				});
			});
		}
	});

	websocket.on("device_status", (data: unknown) => {
		const msg = data as { deviceId?: string; target: string; on: boolean; success: boolean; overrideActive?: boolean; overrideRemainingMs?: number };
		const device = msg.deviceId 
			? devices.find((d) => d.id === msg.deviceId)
			: devices.find((d) => d.ipAddress === msg.target);
		if (device) {
			if (msg.success) device.isOn = msg.on;
			pendingDevices.delete(device.id);
			if (msg.overrideActive && msg.overrideRemainingMs) {
				overriddenDevices[device.id] = Date.now() + msg.overrideRemainingMs;
			}
		}
	});

	websocket.send("get_devices", {});
}
