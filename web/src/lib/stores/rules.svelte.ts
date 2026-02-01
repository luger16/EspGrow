import type { AutomationRule } from "$lib/types";

export const rules = $state<AutomationRule[]>([
	{
		id: "rule-1",
		name: "High Temperature Fan",
		enabled: true,
		sensorId: "sensor-1",
		operator: ">",
		threshold: 28,
		deviceId: "device-1",
		action: "turn_on",
	},
	{
		id: "rule-2",
		name: "Low Humidity Humidifier",
		enabled: true,
		sensorId: "sensor-2",
		operator: "<",
		threshold: 50,
		deviceId: "device-4",
		action: "turn_on",
	},
	{
		id: "rule-3",
		name: "Night Heater",
		enabled: false,
		sensorId: "sensor-1",
		operator: "<",
		threshold: 18,
		deviceId: "device-3",
		action: "turn_on",
	},
]);

export function addRule(rule: AutomationRule): void {
	rules.push(rule);
}

export function updateRule(ruleId: string, updates: Partial<Omit<AutomationRule, "id">>): void {
	const rule = rules.find((r) => r.id === ruleId);
	if (rule) {
		Object.assign(rule, updates);
	}
}

export function removeRule(ruleId: string): void {
	const index = rules.findIndex((r) => r.id === ruleId);
	if (index !== -1) {
		rules.splice(index, 1);
	}
}

export function toggleRule(ruleId: string): void {
	const rule = rules.find((r) => r.id === ruleId);
	if (rule) {
		rule.enabled = !rule.enabled;
	}
}
