import type { Device } from "$lib/types";
import { websocket } from "./websocket.svelte";

export const devices = $state<Device[]>([
	{
		id: "device-1",
		name: "Exhaust Fan",
		type: "fan",
		controlMethod: "relay",
		gpioPin: 16,
		isOn: false,
		controlMode: "automatic",
	},
	{
		id: "device-2",
		name: "Grow Light",
		type: "light",
		controlMethod: "shelly",
		ipAddress: "192.168.1.50",
		isOn: false,
		controlMode: "automatic",
	},
	{
		id: "device-3",
		name: "Space Heater",
		type: "heater",
		controlMethod: "tasmota",
		ipAddress: "192.168.0.74",
		isOn: false,
		controlMode: "manual",
	},
	{
		id: "device-4",
		name: "Humidifier",
		type: "humidifier",
		controlMethod: "relay",
		gpioPin: 17,
		isOn: false,
		controlMode: "automatic",
	},
]);

function getDeviceTarget(device: Device): string {
	if (device.controlMethod === "relay") {
		return String(device.gpioPin ?? 0);
	}
	return device.ipAddress ?? "";
}

export function toggleDevice(deviceId: string): void {
	const device = devices.find((d) => d.id === deviceId);
	if (!device) return;

	const newState = !device.isOn;
	
	websocket.send("device_control", {
		method: device.controlMethod,
		target: getDeviceTarget(device),
		on: newState,
	});
	
	device.isOn = newState;
}

export function setDeviceState(deviceId: string, on: boolean): void {
	const device = devices.find((d) => d.id === deviceId);
	if (!device) return;

	websocket.send("device_control", {
		method: device.controlMethod,
		target: getDeviceTarget(device),
		on,
	});
	
	device.isOn = on;
}

export function setDeviceControlMode(deviceId: string, mode: Device["controlMode"]): void {
	const device = devices.find((d) => d.id === deviceId);
	if (device) {
		device.controlMode = mode;
	}
}

export function addDevice(device: Device): void {
	devices.push(device);
}

export function removeDevice(deviceId: string): void {
	const index = devices.findIndex((d) => d.id === deviceId);
	if (index !== -1) {
		devices.splice(index, 1);
	}
}

export function updateDevice(deviceId: string, updates: Partial<Omit<Device, "id">>): void {
	const device = devices.find((d) => d.id === deviceId);
	if (device) {
		Object.assign(device, updates);
	}
}

export function initDeviceWebSocket(): void {
	websocket.on("device_status", (data: unknown) => {
		const msg = data as { target: string; on: boolean; success: boolean };
		if (!msg.success) {
			console.error("[Devices] Control failed for", msg.target);
		}
	});
}
