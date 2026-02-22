<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { addRule } from "$lib/stores/rules.svelte";
	import { sensors } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import type { AutomationRule, ComparisonOperator } from "$lib/types";
	import PlusIcon from "@lucide/svelte/icons/plus";
	import RuleForm from "./rule-form.svelte";

	let open = $state(false);
	let submitted = $state(false);
	let ruleType = $state<"sensor" | "schedule">("sensor");
	let name = $state("");
	let sensorId = $state("");
	let operator = $state<ComparisonOperator>(">");
	let threshold = $state("");
	let thresholdOff = $state("");
	let onTime = $state("");
	let offTime = $state("");
	let minRunTimeMin = $state("");
	let deviceId = $state("");
	let action = $state<AutomationRule["action"]>("turn_on");

	const canCreateRule = $derived(sensors.length > 0 && devices.length > 0);

	function handleSubmit(e: Event) {
		e.preventDefault();
		submitted = true;
		if (!isValid) return;
		
		const rule: AutomationRule = {
			id: `rule-${Date.now()}`,
			name,
			enabled: true,
			type: ruleType,
			deviceId,
			action,
		};
		
		if (ruleType === "sensor") {
			rule.sensorId = sensorId;
			rule.operator = operator;
			rule.threshold = parseFloat(threshold);
			
			if (thresholdOff) {
				rule.useHysteresis = true;
				rule.thresholdOff = parseFloat(thresholdOff);
			}
			
			if (minRunTimeMin) {
				rule.minRunTimeMs = parseInt(minRunTimeMin) * 60000;
			}
		} else {
			rule.onTime = onTime;
			rule.offTime = offTime;
		}
		
		addRule(rule);
		resetForm();
		open = false;
	}

	function resetForm() {
		submitted = false;
		ruleType = "sensor";
		name = "";
		sensorId = "";
		operator = ">";
		threshold = "";
		thresholdOff = "";
		onTime = "";
		offTime = "";
		minRunTimeMin = "";
		deviceId = "";
		action = "turn_on";
	}

	const isValid = $derived(
		name && 
		deviceId && 
		(ruleType === "sensor" 
			? (sensorId && threshold)
			: (onTime && offTime)
		)
	);

</script>

<Dialog.Root bind:open>
	<Dialog.Trigger>
		{#snippet child({ props })}
			<Button {...props} variant="outline" size="sm" disabled={!canCreateRule}>
				<PlusIcon class="size-4" />
				Add Rule
			</Button>
		{/snippet}
	</Dialog.Trigger>
	<Dialog.Content class="sm:max-w-md">
		<Dialog.Header>
			<Dialog.Title>Add Automation Rule</Dialog.Title>
			<Dialog.Description>Create a rule to automate device control.</Dialog.Description>
		</Dialog.Header>
		<form onsubmit={handleSubmit} class="grid gap-4 py-4">
			<RuleForm
				bind:ruleType
				bind:name
				bind:sensorId
				bind:operator
				bind:threshold
				bind:thresholdOff
				bind:onTime
				bind:offTime
				bind:minRunTimeMin
				bind:deviceId
				bind:action
				{submitted}
			/>
			<Dialog.Footer>
				<Button type="submit" disabled={!isValid}>Add Rule</Button>
			</Dialog.Footer>
		</form>
	</Dialog.Content>
</Dialog.Root>
