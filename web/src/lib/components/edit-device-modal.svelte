<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as AlertDialog from "$lib/components/ui/alert-dialog/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { updateDevice, removeDevice } from "$lib/stores/devices.svelte";
	import type { Device } from "$lib/types";
	import DeviceFormFields from "./device-form-fields.svelte";

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
	let hasEnergyMonitoring = $state(false);
	let showDeleteConfirm = $state(false);

	$effect(() => {
		if (open) {
			submitted = false;
			name = device.name;
			deviceType = device.type;
			controlMethod = device.controlMethod;
			ipAddress = device.ipAddress ?? "";
			hasEnergyMonitoring = device.hasEnergyMonitoring ?? false;
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
			hasEnergyMonitoring,
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
			<DeviceFormFields
				bind:name
				bind:deviceType
				bind:controlMethod
				bind:ipAddress
				bind:hasEnergyMonitoring
				{submitted}
				energyId="energy-edit"
			/>
			<Dialog.Footer class="flex-col gap-2 sm:flex-row sm:justify-between">
				<Button type="button" variant="destructive" onclick={() => (showDeleteConfirm = true)}
					>Delete</Button
				>
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
