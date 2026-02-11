<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as Select from "$lib/components/ui/select/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { addDevice } from "$lib/stores/devices.svelte";
	import type { Device } from "$lib/types";
	import PlusIcon from "@lucide/svelte/icons/plus";

	let open = $state(false);
	let submitted = $state(false);
	let name = $state("");
	let deviceType = $state<Device["type"]>("fan");
	let controlMethod = $state<Device["controlMethod"]>("shelly_gen2");
	let ipAddress = $state("");

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

	function handleSubmit() {
		submitted = true;
		if (!name || !ipAddress) return;
		const device: Device = {
			id: `device-${Date.now()}`,
			name,
			type: deviceType,
			controlMethod,
			ipAddress,
			isOn: false,
			controlMode: "manual",
		};
		addDevice(device);
		resetForm();
		open = false;
	}

	function resetForm() {
		submitted = false;
		name = "";
		deviceType = "fan";
		controlMethod = "shelly_gen2";
		ipAddress = "";
	}
</script>

<Dialog.Root bind:open>
	<Dialog.Trigger>
		{#snippet child({ props })}
			<Button {...props} variant="outline" size="sm">
				<PlusIcon class="size-4" />
				Add Device
			</Button>
		{/snippet}
	</Dialog.Trigger>
	<Dialog.Content class="sm:max-w-md">
		<Dialog.Header>
			<Dialog.Title>Add Device</Dialog.Title>
			<Dialog.Description>Configure a new device for control.</Dialog.Description>
		</Dialog.Header>
		<form onsubmit={handleSubmit} class="grid gap-4 py-4">
			<div class="grid gap-2">
				<Label for="name">Name</Label>
				<Input id="name" bind:value={name} placeholder="e.g. Exhaust Fan" required />
				{#if submitted && !name}
					<p class="text-destructive text-xs">Name is required</p>
				{/if}
			</div>
			<div class="grid gap-2">
				<Label>Device Type</Label>
				<Select.Root type="single" value={deviceType} onValueChange={(v) => v && (deviceType = v as Device["type"])}>
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
				<Select.Root type="single" value={controlMethod} onValueChange={(v) => v && (controlMethod = v as Device["controlMethod"])}>
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
			<Dialog.Footer>
				<Button type="submit" disabled={!name || !ipAddress}>Add Device</Button>
			</Dialog.Footer>
		</form>
	</Dialog.Content>
</Dialog.Root>
