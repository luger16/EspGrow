<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as Select from "$lib/components/ui/select/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { addRule } from "$lib/stores/rules.svelte";
	import { sensors } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import type { AutomationRule, ComparisonOperator } from "$lib/types";
	import PlusIcon from "@lucide/svelte/icons/plus";

	let open = $state(false);
	let name = $state("");
	let sensorId = $state("");
	let operator = $state<ComparisonOperator>(">");
	let threshold = $state("");
	let deviceId = $state("");
	let action = $state<AutomationRule["action"]>("turn_on");

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
	const canCreateRule = $derived(sensors.length > 0 && devices.length > 0);

	function handleSubmit() {
		const rule: AutomationRule = {
			id: `rule-${Date.now()}`,
			name,
			enabled: true,
			sensorId,
			operator,
			threshold: parseFloat(threshold),
			deviceId,
			action,
		};
		addRule(rule);
		resetForm();
		open = false;
	}

	function resetForm() {
		name = "";
		sensorId = "";
		operator = ">";
		threshold = "";
		deviceId = "";
		action = "turn_on";
	}

	const isValid = $derived(name && sensorId && threshold && deviceId);
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
			<Dialog.Description>Create a rule to automate device control based on sensor readings.</Dialog.Description>
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

			<Dialog.Footer>
				<Button type="submit" disabled={!isValid}>Add Rule</Button>
			</Dialog.Footer>
		</form>
	</Dialog.Content>
</Dialog.Root>
