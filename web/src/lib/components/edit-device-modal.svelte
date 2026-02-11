<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as AlertDialog from "$lib/components/ui/alert-dialog/index.js";
	import * as Select from "$lib/components/ui/select/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { updateDevice, removeDevice } from "$lib/stores/devices.svelte";
	import type { Device } from "$lib/types";

	type Props = {
		device: Device;
		open: boolean;
		onOpenChange: (open: boolean) => void;
	};
	let { device, open = $bindable(), onOpenChange }: Props = $props();

	let name = $state("");
	let submitted = $state(false);
	let deviceType = $state<Device["type"]>("fan");
	let controlMethod = $state<Device["controlMethod"]>("shelly_gen2");
	let ipAddress = $state("");
	let showDeleteConfirm = $state(false);

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

	$effect(() => {
		if (open) {
			submitted = false;
			name = device.name;
			deviceType = device.type;
			controlMethod = device.controlMethod;
			ipAddress = device.ipAddress ?? "";
		}
	});

	function handleSubmit() {
		submitted = true;
		if (!name || !ipAddress) return;
		updateDevice(device.id, {
			name,
			type: deviceType,
			controlMethod,
			ipAddress,
		});
		onOpenChange(false);
	}

	function handleDelete() {
		removeDevice(device.id);
		showDeleteConfirm = false;
		onOpenChange(false);
	}
</script>

<Dialog.Root {open} {onOpenChange}>
	<Dialog.Content class="sm:max-w-md">
		<Dialog.Header>
			<Dialog.Title>Edit Device</Dialog.Title>
			<Dialog.Description>Update device configuration.</Dialog.Description>
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
			<Dialog.Footer class="flex-col gap-2 sm:flex-row sm:justify-between">
				<Button type="button" variant="destructive" onclick={() => (showDeleteConfirm = true)}>Delete</Button>
				<Button type="submit" disabled={!name || !ipAddress}>Save Changes</Button>
			</Dialog.Footer>
		</form>
	</Dialog.Content>
</Dialog.Root>

<AlertDialog.Root bind:open={showDeleteConfirm}>
	<AlertDialog.Content>
		<AlertDialog.Header>
			<AlertDialog.Title>Delete Device</AlertDialog.Title>
			<AlertDialog.Description>
				Are you sure you want to delete "{device.name}"? This action cannot be undone.
			</AlertDialog.Description>
		</AlertDialog.Header>
		<AlertDialog.Footer>
			<AlertDialog.Cancel>Cancel</AlertDialog.Cancel>
			<AlertDialog.Action onclick={handleDelete}>Delete</AlertDialog.Action>
		</AlertDialog.Footer>
	</AlertDialog.Content>
</AlertDialog.Root>
