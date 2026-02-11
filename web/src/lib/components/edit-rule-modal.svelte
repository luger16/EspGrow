<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as AlertDialog from "$lib/components/ui/alert-dialog/index.js";
	import * as Select from "$lib/components/ui/select/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { Switch } from "$lib/components/ui/switch/index.js";
	import { updateRule, removeRule } from "$lib/stores/rules.svelte";
	import { sensors } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import type { AutomationRule, ComparisonOperator } from "$lib/types";

	type Props = {
		rule: AutomationRule;
		open: boolean;
		onOpenChange: (open: boolean) => void;
	};
	let { rule, open = $bindable(), onOpenChange }: Props = $props();

	let name = $state("");
	let submitted = $state(false);
	let sensorId = $state("");
	let operator = $state<ComparisonOperator>(">");
	let threshold = $state("");
	let useHysteresis = $state(false);
	let thresholdOff = $state("");
	let minRunTimeMs = $state("");
	let deviceId = $state("");
	let action = $state<AutomationRule["action"]>("turn_on");
	let showDeleteConfirm = $state(false);

	const operatorOptions: { value: ComparisonOperator; label: string }[] = [
		{ value: ">", label: ">" },
		{ value: ">=", label: ">=" },
		{ value: "<", label: "<" },
		{ value: "<=", label: "<=" },
		{ value: "=", label: "=" },
	];

	const actionOptions: { value: AutomationRule["action"]; label: string }[] = [
		{ value: "turn_on", label: "Turn On" },
		{ value: "turn_off", label: "Turn Off" },
	];

	const selectedSensor = $derived(sensors.find((s) => s.id === sensorId));

	$effect(() => {
		if (open) {
			submitted = false;
			name = rule.name;
			sensorId = rule.sensorId;
			operator = rule.operator;
			threshold = rule.threshold.toString();
			useHysteresis = rule.useHysteresis ?? false;
			thresholdOff = rule.thresholdOff?.toString() ?? "";
			minRunTimeMs = rule.minRunTimeMs ? (rule.minRunTimeMs / 60000).toString() : "";
			deviceId = rule.deviceId;
			action = rule.action;
		}
	});

	function handleSubmit() {
		submitted = true;
		if (!isValid) return;
		const updates: Partial<AutomationRule> = {
			name,
			sensorId,
			operator,
			threshold: parseFloat(threshold),
			deviceId,
			action,
		};
		
		if (useHysteresis && thresholdOff) {
			updates.useHysteresis = true;
			updates.thresholdOff = parseFloat(thresholdOff);
		} else {
			updates.useHysteresis = false;
			updates.thresholdOff = undefined;
		}
		
		if (minRunTimeMs) {
			updates.minRunTimeMs = parseInt(minRunTimeMs) * 60000;
		} else {
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
		sensorId && 
		threshold && 
		deviceId && 
		(!useHysteresis || thresholdOff)
	);

</script>

<Dialog.Root {open} {onOpenChange}>
	<Dialog.Content class="sm:max-w-md">
		<Dialog.Header>
			<Dialog.Title>Edit Rule</Dialog.Title>
			<Dialog.Description>Update automation rule configuration.</Dialog.Description>
		</Dialog.Header>
		<form onsubmit={handleSubmit} class="grid gap-4 py-4">
			<div class="grid gap-2">
				<Label for="name">Rule Name</Label>
				<Input id="name" bind:value={name} placeholder="e.g. High Temperature Fan" required />
				{#if submitted && !name}
					<p class="text-destructive text-xs">Rule name is required</p>
				{/if}
			</div>

			<div class="grid gap-2">
				<Label>When</Label>
				<Select.Root type="single" value={sensorId} onValueChange={(v) => v && (sensorId = v)}>
					<Select.Trigger>
						<span>{selectedSensor?.name ?? "Select sensor..."}</span>
					</Select.Trigger>
					<Select.Content>
						{#each sensors as sensor (sensor.id)}
							<Select.Item value={sensor.id}>{sensor.name}</Select.Item>
						{/each}
					</Select.Content>
				</Select.Root>
				{#if submitted && !sensorId}
					<p class="text-destructive text-xs">Select a sensor</p>
				{/if}
			</div>

			<div class="grid grid-cols-[80px_1fr] gap-2">
				<Select.Root type="single" value={operator} onValueChange={(v) => v && (operator = v as ComparisonOperator)}>
					<Select.Trigger>
						<span>{operator}</span>
					</Select.Trigger>
					<Select.Content>
						{#each operatorOptions as opt (opt.value)}
							<Select.Item value={opt.value}>{opt.label}</Select.Item>
						{/each}
					</Select.Content>
				</Select.Root>
				<Input
					type="number"
					bind:value={threshold}
					placeholder={selectedSensor ? `Value (${selectedSensor.unit})` : "Value"}
					required
				/>
			</div>
			{#if submitted && !threshold}
				<p class="text-destructive text-xs -mt-2">Threshold value is required</p>
			{/if}

			<div class="grid gap-3 rounded-lg border p-3">
				<div class="flex items-center justify-between">
					<div class="space-y-0.5">
						<Label class="text-sm font-medium">Enable Hysteresis</Label>
						<p class="text-xs text-muted-foreground">Use two thresholds to prevent rapid switching</p>
					</div>
					<Switch bind:checked={useHysteresis} />
				</div>
				
				{#if useHysteresis}
					<div class="grid gap-2">
						<Label for="thresholdOff">Turn Off Threshold</Label>
						<Input
							id="thresholdOff"
							type="number"
							bind:value={thresholdOff}
							placeholder={selectedSensor ? `Value (${selectedSensor.unit})` : "Value"}
							required={useHysteresis}
						/>
						{#if submitted && useHysteresis && !thresholdOff}
							<p class="text-destructive text-xs">Turn off threshold is required</p>
						{/if}
						<p class="text-xs text-muted-foreground">
							Device turns on at {threshold || "___"}{selectedSensor?.unit || ""}, turns off at {thresholdOff || "___"}{selectedSensor?.unit || ""}
						</p>
					</div>
				{/if}
			</div>

			<div class="grid gap-2">
				<Label for="minRunTime">Minimum Run Time (minutes)</Label>
				<Input
					id="minRunTime"
					type="number"
					bind:value={minRunTimeMs}
					placeholder="Optional (e.g. 2)"
				/>
				<p class="text-xs text-muted-foreground">Device must stay in new state for at least this long</p>
			</div>

			<div class="grid gap-2">
				<Label>Then</Label>
				<div class="grid grid-cols-[1fr_120px] gap-2">
					<Select.Root type="single" value={deviceId} onValueChange={(v) => v && (deviceId = v)}>
						<Select.Trigger>
							<span>{devices.find((d) => d.id === deviceId)?.name ?? "Select device..."}</span>
						</Select.Trigger>
						<Select.Content>
							{#each devices as device (device.id)}
								<Select.Item value={device.id}>{device.name}</Select.Item>
							{/each}
						</Select.Content>
					</Select.Root>
					<Select.Root type="single" value={action} onValueChange={(v) => v && (action = v as AutomationRule["action"])}>
						<Select.Trigger>
							<span>{actionOptions.find((a) => a.value === action)?.label}</span>
						</Select.Trigger>
						<Select.Content>
							{#each actionOptions as opt (opt.value)}
								<Select.Item value={opt.value}>{opt.label}</Select.Item>
							{/each}
						</Select.Content>
					</Select.Root>
				</div>
				{#if submitted && !deviceId}
					<p class="text-destructive text-xs">Select a device</p>
				{/if}
			</div>

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
