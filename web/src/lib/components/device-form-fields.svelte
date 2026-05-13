<script lang="ts">
	import * as Select from "$lib/components/ui/select/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { Checkbox } from "$lib/components/ui/checkbox/index.js";
	import type { Device } from "$lib/types";

	type Props = {
		name: string;
		deviceType: Device["type"];
		controlMethod: Device["controlMethod"];
		ipAddress: string;
		hasEnergyMonitoring: boolean;
		submitted: boolean;
		energyId?: string;
	};
	let {
		name = $bindable(),
		deviceType = $bindable(),
		controlMethod = $bindable(),
		ipAddress = $bindable(),
		hasEnergyMonitoring = $bindable(),
		submitted,
		energyId = "energy",
	}: Props = $props();

	const deviceTypeOptions: { value: Device["type"]; label: string }[] = [
		{ value: "fan", label: "Fan" },
		{ value: "light", label: "Light" },
		{ value: "heater", label: "Heater" },
		{ value: "pump", label: "Pump" },
		{ value: "humidifier", label: "Humidifier" },
	];

	const controlMethodOptions: { value: Device["controlMethod"]; label: string }[] = [
		{ value: "shelly_gen1", label: "Shelly Gen1 (HTTP API)" },
		{ value: "shelly_gen2", label: "Shelly Gen2/Plus (RPC API)" },
		{ value: "tasmota", label: "Tasmota (HTTP API)" },
	];
</script>

<div class="grid gap-2">
	<Label for="name">Name</Label>
	<Input id="name" bind:value={name} placeholder="e.g. Exhaust Fan" required />
	{#if submitted && !name}
		<p class="text-destructive text-xs">Name is required</p>
	{/if}
</div>
<div class="grid gap-2">
	<Label>Device Type</Label>
	<Select.Root
		type="single"
		value={deviceType}
		onValueChange={(v) => v && (deviceType = v as Device["type"])}
	>
		<Select.Trigger>
			<span>{deviceTypeOptions.find((o) => o.value === deviceType)?.label}</span>
		</Select.Trigger>
		<Select.Content>
			{#each deviceTypeOptions as option (option.value)}
				<Select.Item value={option.value}>{option.label}</Select.Item>
			{/each}
		</Select.Content>
	</Select.Root>
</div>
<div class="grid gap-2">
	<Label>Control Method</Label>
	<Select.Root
		type="single"
		value={controlMethod}
		onValueChange={(v) => v && (controlMethod = v as Device["controlMethod"])}
	>
		<Select.Trigger>
			<span>{controlMethodOptions.find((o) => o.value === controlMethod)?.label}</span>
		</Select.Trigger>
		<Select.Content>
			{#each controlMethodOptions as option (option.value)}
				<Select.Item value={option.value}>{option.label}</Select.Item>
			{/each}
		</Select.Content>
	</Select.Root>
</div>
<div class="grid gap-2">
	<Label for="ip">IP Address</Label>
	<Input id="ip" bind:value={ipAddress} placeholder="e.g. 192.168.1.50" required />
	{#if submitted && !ipAddress}
		<p class="text-destructive text-xs">IP address is required</p>
	{/if}
</div>
<div class="flex items-center gap-2">
	<Checkbox id={energyId} bind:checked={hasEnergyMonitoring} />
	<Label for={energyId} class="font-normal">Energy monitoring</Label>
</div>
