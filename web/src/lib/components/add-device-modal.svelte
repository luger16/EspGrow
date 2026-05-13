<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { addDevice } from "$lib/stores/devices.svelte";
	import type { Device } from "$lib/types";
	import PlusIcon from "@lucide/svelte/icons/plus";
	import DeviceFormFields from "./device-form-fields.svelte";

	let open = $state(false);
	let submitted = $state(false);
	let name = $state("");
	let deviceType = $state<Device["type"]>("fan");
	let controlMethod = $state<Device["controlMethod"]>("shelly_gen2");
	let ipAddress = $state("");
	let hasEnergyMonitoring = $state(false);

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
			hasEnergyMonitoring,
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
		hasEnergyMonitoring = false;
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
			<DeviceFormFields
				bind:name
				bind:deviceType
				bind:controlMethod
				bind:ipAddress
				bind:hasEnergyMonitoring
				{submitted}
				energyId="energy"
			/>
			<Dialog.Footer>
				<Button type="submit" disabled={!name || !ipAddress}>Add Device</Button>
			</Dialog.Footer>
		</form>
	</Dialog.Content>
</Dialog.Root>
