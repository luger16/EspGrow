import type { AutomationRule } from "$lib/types";
import { websocket } from "./websocket.svelte";
import { devices } from "./devices.svelte";

export const rules = $state<AutomationRule[]>([]);

function getDeviceTarget(deviceId: string): { method: string; target: string } {
	const device = devices.find((d) => d.id === deviceId);
	if (!device) return { method: "", target: "" };
	
	return { method: device.controlMethod, target: device.ipAddress ?? "" };
}

export function addRule(rule: AutomationRule): void {
	const { method, target } = getDeviceTarget(rule.deviceId);
	
	websocket.send("add_rule", {
		id: rule.id,
		name: rule.name,
		enabled: rule.enabled,
		ruleType: rule.type,
		sensorId: rule.sensorId,
		operator: rule.operator,
		threshold: rule.threshold,
		thresholdOff: rule.thresholdOff,
		useHysteresis: rule.useHysteresis,
		onTime: rule.onTime,
		offTime: rule.offTime,
		minRunTimeMs: rule.minRunTimeMs,
		deviceId: rule.deviceId,
		deviceMethod: method,
		deviceTarget: target,
		action: rule.action,
	});
}

export function updateRule(ruleId: string, updates: Partial<Omit<AutomationRule, "id">>): void {
	const { type: ruleType, ...rest } = updates;
	const payload: Record<string, unknown> = { id: ruleId, ...rest };
	
	if (ruleType) {
		payload.ruleType = ruleType;
	}
	
	if (updates.deviceId) {
		const { method, target } = getDeviceTarget(updates.deviceId);
		payload.deviceMethod = method;
		payload.deviceTarget = target;
	}
	
	websocket.send("update_rule", payload);
}

export function removeRule(ruleId: string): void {
	websocket.send("remove_rule", { id: ruleId });
}

export function toggleRule(ruleId: string): void {
	websocket.send("toggle_rule", { id: ruleId });
}

export function initRulesWebSocket(): void {
	websocket.on("rules", (data: unknown) => {
		const msg = data as { data: AutomationRule[] };
		if (Array.isArray(msg.data)) {
			rules.length = 0;
			rules.push(...msg.data);
		}
	});
	
	websocket.send("get_rules", {});
}
