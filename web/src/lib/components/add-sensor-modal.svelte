<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { addSensor } from "$lib/stores/sensors.svelte";
	import type { Sensor } from "$lib/types";
	import PlusIcon from "@lucide/svelte/icons/plus";
	import SensorFormFields, { sensorTypeOptions } from "./sensor-form-fields.svelte";

	let open = $state(false);
	let submitted = $state(false);
	let name = $state("");
	let hardwareType = $state<Sensor["hardwareType"]>("sht3x");
	let sensorType = $state<Sensor["type"]>("temperature");
	let address = $state("");
	let tempSourceId = $state("");
	let humSourceId = $state("");
	let leafTempOffset = $state(2);

	const needsSources = $derived(
		hardwareType === "calculated" && (sensorType === "vpd" || sensorType === "dewpoint")
	);
	const isVpd = $derived(hardwareType === "calculated" && sensorType === "vpd");

	function handleSubmit() {
		submitted = true;
		if (!name) return;
		if (needsSources && (!tempSourceId || !humSourceId)) return;
		const typeOption = sensorTypeOptions.find((o) => o.value === sensorType);
		const sensor: Sensor = {
			id: `sensor-${Date.now()}`,
			name,
			type: sensorType,
			unit: typeOption?.unit ?? "",
			hardwareType,
			address: address || undefined,
			tempSourceId: needsSources ? tempSourceId : undefined,
			humSourceId: needsSources ? humSourceId : undefined,
			leafTempOffset: isVpd ? leafTempOffset : undefined,
		};
		addSensor(sensor);
		resetForm();
		open = false;
	}

	function resetForm() {
		submitted = false;
		name = "";
		hardwareType = "sht3x";
		sensorType = "temperature";
		address = "";
		tempSourceId = "";
		humSourceId = "";
		leafTempOffset = 2;
	}
</script>

<Dialog.Root bind:open>
	<Dialog.Trigger>
		{#snippet child({ props })}
			<Button {...props} variant="outline" size="sm">
				<PlusIcon class="size-4" />
				Add Sensor
			</Button>
		{/snippet}
	</Dialog.Trigger>
	<Dialog.Content class="sm:max-w-md">
		<Dialog.Header>
			<Dialog.Title>Add Sensor</Dialog.Title>
			<Dialog.Description>Configure a new sensor for monitoring.</Dialog.Description>
		</Dialog.Header>
		<form onsubmit={handleSubmit} class="grid gap-4 py-4">
			<SensorFormFields
				bind:name
				bind:hardwareType
				bind:sensorType
				bind:address
				bind:tempSourceId
				bind:humSourceId
				bind:leafTempOffset
				{submitted}
			/>
			<Dialog.Footer>
				<Button type="submit" disabled={!name || (needsSources && (!tempSourceId || !humSourceId))}
					>Add Sensor</Button
				>
			</Dialog.Footer>
		</form>
	</Dialog.Content>
</Dialog.Root>
