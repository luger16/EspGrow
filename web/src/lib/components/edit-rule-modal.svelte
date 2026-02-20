<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as AlertDialog from "$lib/components/ui/alert-dialog/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { updateRule, removeRule } from "$lib/stores/rules.svelte";
	import type { AutomationRule, ComparisonOperator } from "$lib/types";
	import RuleForm from "./rule-form.svelte";

	type Props = {
		rule: AutomationRule;
		open: boolean;
		onOpenChange: (open: boolean) => void;
	};
	let { rule, open = $bindable(), onOpenChange }: Props = $props();

	let ruleType = $state<"sensor" | "schedule">("sensor");
	let name = $state("");
	let submitted = $state(false);
	let sensorId = $state("");
	let operator = $state<ComparisonOperator>(">");
	let threshold = $state("");
	let useHysteresis = $state(false);
	let thresholdOff = $state("");
	let onTime = $state("");
	let offTime = $state("");
	let minRunTimeMin = $state("");
	let deviceId = $state("");
	let action = $state<AutomationRule["action"]>("turn_on");
	let showDeleteConfirm = $state(false);
	let initializing = $state(false);

	$effect(() => {
		if (open) {
			initializing = true;
			submitted = false;
			ruleType = rule.type;
			name = rule.name;
			sensorId = rule.sensorId ?? "";
			operator = rule.operator ?? ">";
			threshold = rule.threshold?.toString() ?? "";
			useHysteresis = rule.useHysteresis ?? false;
			thresholdOff = rule.thresholdOff?.toString() ?? "";
			onTime = rule.onTime ?? "";
			offTime = rule.offTime ?? "";
			minRunTimeMin = rule.minRunTimeMs ? (rule.minRunTimeMs / 60000).toString() : "";
			deviceId = rule.deviceId;
			action = rule.action;
			initializing = false;
		}
	});
	
	$effect(() => {
		if (initializing) return;
		if (ruleType === "sensor") {
			onTime = "";
			offTime = "";
		} else {
			sensorId = "";
			operator = ">";
			threshold = "";
			useHysteresis = false;
			thresholdOff = "";
			minRunTimeMin = "";
		}
	});

	function handleSubmit(e: Event) {
		e.preventDefault();
		submitted = true;
		if (!isValid) return;
		const updates: Partial<AutomationRule> = {
			type: ruleType,
			name,
			deviceId,
			action,
		};
		
		if (ruleType === "sensor") {
			updates.sensorId = sensorId;
			updates.operator = operator;
			updates.threshold = parseFloat(threshold);
			
			if (useHysteresis && thresholdOff) {
				updates.useHysteresis = true;
				updates.thresholdOff = parseFloat(thresholdOff);
			} else {
				updates.useHysteresis = false;
				updates.thresholdOff = undefined;
			}
			
			if (minRunTimeMin) {
				updates.minRunTimeMs = parseInt(minRunTimeMin) * 60000;
			} else {
				updates.minRunTimeMs = undefined;
			}
			
			// Clear schedule fields
			updates.onTime = undefined;
			updates.offTime = undefined;
		} else {
			updates.onTime = onTime;
			updates.offTime = offTime;
			
			// Clear sensor fields
			updates.sensorId = undefined;
			updates.operator = undefined;
			updates.threshold = undefined;
			updates.useHysteresis = undefined;
			updates.thresholdOff = undefined;
			updates.minRunTimeMs = undefined;
		}
		
		updateRule(rule.id, updates);
		onOpenChange(false);
	}

	function handleDelete() {
		removeRule(rule.id);
		showDeleteConfirm = false;
		onOpenChange(false);
	}

	const isValid = $derived(
		name && 
		deviceId && 
		(ruleType === "sensor" 
			? (sensorId && threshold && (!useHysteresis || thresholdOff))
			: (onTime && offTime)
		)
	);

</script>

<Dialog.Root {open} {onOpenChange}>
	<Dialog.Content class="sm:max-w-md">
		<Dialog.Header>
			<Dialog.Title>Edit Rule</Dialog.Title>
			<Dialog.Description>Update automation rule configuration.</Dialog.Description>
		</Dialog.Header>
		<form onsubmit={handleSubmit} class="grid gap-4 py-4">
			<RuleForm
				bind:ruleType
				bind:name
				bind:sensorId
				bind:operator
				bind:threshold
				bind:useHysteresis
				bind:thresholdOff
				bind:onTime
				bind:offTime
				bind:minRunTimeMin
				bind:deviceId
				bind:action
				{submitted}
			/>
			<Dialog.Footer class="flex-col gap-2 sm:flex-row sm:justify-between">
				<Button type="button" variant="destructive" onclick={() => (showDeleteConfirm = true)}>Delete</Button>
				<Button type="submit" disabled={!isValid}>Save Changes</Button>
			</Dialog.Footer>
		</form>
	</Dialog.Content>
</Dialog.Root>

<AlertDialog.Root bind:open={showDeleteConfirm}>
	<AlertDialog.Content>
		<AlertDialog.Header>
			<AlertDialog.Title>Delete Rule</AlertDialog.Title>
			<AlertDialog.Description>
				Are you sure you want to delete "{rule.name}"? This action cannot be undone.
			</AlertDialog.Description>
		</AlertDialog.Header>
		<AlertDialog.Footer>
			<AlertDialog.Cancel>Cancel</AlertDialog.Cancel>
			<AlertDialog.Action onclick={handleDelete}>Delete</AlertDialog.Action>
		</AlertDialog.Footer>
	</AlertDialog.Content>
</AlertDialog.Root>
