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

	if (!websocket.connected) {
		console.error("Cannot toggle device: WebSocket disconnected");
		return;
	}

	pendingDevices.add(deviceId);
	const newState = !device.isOn;
	
	websocket.send("device_control", {
		method: device.controlMethod,
		target: getDeviceTarget(device),
		on: newState,
	});

	// Safety timeout to clear pending state if no response
	setTimeout(() => {
		if (pendingDevices.has(deviceId)) {
			pendingDevices.delete(deviceId);
		}
	}, 5000);
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

export function clearOverride(deviceId: string): void {
	websocket.send("clear_override", { deviceId });
}

export function initDeviceWebSocket(): void {
	websocket.on("devices", (data: unknown) => {
		if (!Array.isArray(data)) return;
		const items = data.filter(
			(item): item is Device =>
				item && typeof item === "object" && typeof item.id === "string" && typeof item.name === "string"
		);
		devices.length = 0;
		items.forEach((d) => {
			devices.push({
				...d,
				isOn: d.isOn ?? false,
			});
		});
	});

	websocket.on("device_status", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const msg = data as Record<string, unknown>;
		if (typeof msg.on !== "boolean" || typeof msg.success !== "boolean") return;
		
		const device = typeof msg.deviceId === "string"
			? devices.find((d) => d.id === msg.deviceId)
			: typeof msg.target === "string"
				? devices.find((d) => d.ipAddress === msg.target)
				: undefined;
		if (device) {
			if (msg.success) device.isOn = msg.on;
			pendingDevices.delete(device.id);
			if (msg.overrideActive && typeof msg.overrideRemainingMs === "number") {
				overriddenDevices[device.id] = Date.now() + msg.overrideRemainingMs;
			}
		}
	});

	websocket.on("override_cleared", (data: unknown) => {
		if (!data || typeof data !== "object") return;
		const msg = data as Record<string, unknown>;
		if (typeof msg.deviceId === "string" && overriddenDevices[msg.deviceId]) {
			delete overriddenDevices[msg.deviceId];
		}
	});

	websocket.send("get_devices");
}
