import type { Device } from "$lib/types";

export const devices = $state<Device[]>([
	{
		id: "device-1",
		name: "Exhaust Fan",
		type: "fan",
		controlMethod: "relay",
		gpioPin: 16,
		isOn: true,
		controlMode: "automatic",
	},
	{
		id: "device-2",
		name: "Grow Light",
		type: "light",
		controlMethod: "shelly",
		ipAddress: "192.168.1.50",
		isOn: true,
		controlMode: "automatic",
	},
	{
		id: "device-3",
		name: "Space Heater",
		type: "heater",
		controlMethod: "tasmota",
		ipAddress: "192.168.1.51",
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

export function toggleDevice(deviceId: string): void {
	const device = devices.find((d) => d.id === deviceId);
	if (device) {
		device.isOn = !device.isOn;
	}
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
