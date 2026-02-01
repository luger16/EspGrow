<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as AlertDialog from "$lib/components/ui/alert-dialog/index.js";
	import * as Select from "$lib/components/ui/select/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
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
	let sensorId = $state("");
	let operator = $state<ComparisonOperator>(">");
	let threshold = $state("");
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
			name = rule.name;
			sensorId = rule.sensorId;
			operator = rule.operator;
			threshold = rule.threshold.toString();
			deviceId = rule.deviceId;
			action = rule.action;
		}
	});

	function handleSubmit() {
		updateRule(rule.id, {
			name,
			sensorId,
			operator,
			threshold: parseFloat(threshold),
			deviceId,
			action,
		});
		onOpenChange(false);
	}

	function handleDelete() {
		removeRule(rule.id);
		showDeleteConfirm = false;
		onOpenChange(false);
	}

	const isValid = $derived(name && sensorId && threshold && deviceId);
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
