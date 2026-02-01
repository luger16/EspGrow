import type { Sensor, SensorReading, HistoricalReading } from "$lib/types";

function generateHistoricalData(
	baseValue: number,
	variance: number,
	days: number = 7
): HistoricalReading[] {
	const data: HistoricalReading[] = [];
	const now = new Date();

	for (let i = days * 24; i >= 0; i--) {
		const date = new Date(now.getTime() - i * 60 * 60 * 1000);
		const randomVariance = (Math.random() - 0.5) * 2 * variance;
		const value = Math.round((baseValue + randomVariance) * 10) / 10;
		data.push({ date, value });
	}

	return data;
}

export const sensors = $state<Sensor[]>([
	{
		id: "sensor-1",
		name: "Tent Temperature",
		type: "temperature",
		unit: "°C",
		hardwareType: "sht41",
		address: "0x44",
	},
	{
		id: "sensor-2",
		name: "Tent Humidity",
		type: "humidity",
		unit: "%",
		hardwareType: "sht41",
		address: "0x44",
	},
	{
		id: "sensor-3",
		name: "CO₂ Level",
		type: "co2",
		unit: "ppm",
		hardwareType: "scd40",
		address: "0x62",
	},
	{
		id: "sensor-4",
		name: "Light Intensity",
		type: "light",
		unit: "PPFD",
		hardwareType: "as7341",
		address: "0x39",
	},
	{
		id: "sensor-5",
		name: "Soil Moisture",
		type: "soil_moisture",
		unit: "%",
		hardwareType: "soil_capacitive",
		address: "GPIO34",
	},
]);

export const sensorReadings = $state<Record<string, SensorReading>>({
	"sensor-1": { sensorId: "sensor-1", value: 24.5, timestamp: new Date() },
	"sensor-2": { sensorId: "sensor-2", value: 62, timestamp: new Date() },
	"sensor-3": { sensorId: "sensor-3", value: 850, timestamp: new Date() },
	"sensor-4": { sensorId: "sensor-4", value: 420, timestamp: new Date() },
	"sensor-5": { sensorId: "sensor-5", value: 45, timestamp: new Date() },
});

export function getSensorReading(sensorId: string): SensorReading | undefined {
	return sensorReadings[sensorId];
}

export function addSensor(sensor: Sensor): void {
	sensors.push(sensor);
}

export function removeSensor(sensorId: string): void {
	const index = sensors.findIndex((s) => s.id === sensorId);
	if (index !== -1) {
		sensors.splice(index, 1);
		delete sensorReadings[sensorId];
	}
}

export function updateSensor(sensorId: string, updates: Partial<Omit<Sensor, "id">>): void {
	const sensor = sensors.find((s) => s.id === sensorId);
	if (sensor) {
		Object.assign(sensor, updates);
	}
}

export const sensorHistory: Record<string, HistoricalReading[]> = {
	"sensor-1": generateHistoricalData(24.5, 3, 7),
	"sensor-2": generateHistoricalData(62, 10, 7),
	"sensor-3": generateHistoricalData(850, 150, 7),
	"sensor-4": generateHistoricalData(420, 100, 7),
	"sensor-5": generateHistoricalData(45, 15, 7),
};

export function getSensorHistory(sensorId: string): HistoricalReading[] {
	return sensorHistory[sensorId] ?? [];
}
