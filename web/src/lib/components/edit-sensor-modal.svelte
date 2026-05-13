<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as AlertDialog from "$lib/components/ui/alert-dialog/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { updateSensor, removeSensor } from "$lib/stores/sensors.svelte";
	import type { Sensor } from "$lib/types";
	import SensorFormFields, { sensorTypeOptions } from "./sensor-form-fields.svelte";

	type Props = {
		sensor: Sensor;
		open: boolean;
		onOpenChange: (open: boolean) => void;
	};
	let { sensor, open = $bindable(), onOpenChange }: Props = $props();

	let name = $state("");
	let submitted = $state(false);
	let hardwareType = $state<Sensor["hardwareType"]>("sht3x");
	let sensorType = $state<Sensor["type"]>("temperature");
	let address = $state("");
	let tempSourceId = $state("");
	let humSourceId = $state("");
	let leafTempOffset = $state(2);
	let showDeleteConfirm = $state(false);

	const needsSources = $derived(
		hardwareType === "calculated" && (sensorType === "vpd" || sensorType === "dewpoint")
	);
	const isVpd = $derived(hardwareType === "calculated" && sensorType === "vpd");

	$effect(() => {
		if (open) {
			submitted = false;
			name = sensor.name;
			hardwareType = sensor.hardwareType;
			sensorType = sensor.type;
			address = sensor.address ?? "";
			tempSourceId = sensor.tempSourceId ?? "";
			humSourceId = sensor.humSourceId ?? "";
			leafTempOffset = sensor.leafTempOffset ?? 2;
		}
	});

	function handleSubmit() {
		submitted = true;
		if (!name) return;
		if (needsSources && (!tempSourceId || !humSourceId)) return;
		const typeOption = sensorTypeOptions.find((o) => o.value === sensorType);
		updateSensor(sensor.id, {
			name,
			type: sensorType,
			unit: typeOption?.unit ?? "",
			hardwareType,
			address: address || undefined,
			tempSourceId: needsSources ? tempSourceId : undefined,
			humSourceId: needsSources ? humSourceId : undefined,
			leafTempOffset: isVpd ? leafTempOffset : undefined,
		});
		onOpenChange(false);
	}

	function handleDelete() {
		removeSensor(sensor.id);
		showDeleteConfirm = false;
		onOpenChange(false);
	}
</script>

<Dialog.Root {open} {onOpenChange}>
	<Dialog.Content class="sm:max-w-md">
		<Dialog.Header>
			<Dialog.Title>Edit Sensor</Dialog.Title>
			<Dialog.Description>Update sensor configuration.</Dialog.Description>
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
			<Dialog.Footer class="flex-col gap-2 sm:flex-row sm:justify-between">
				<Button type="button" variant="destructive" onclick={() => (showDeleteConfirm = true)}
					>Delete</Button
				>
				<Button type="submit" disabled={!name || (needsSources && (!tempSourceId || !humSourceId))}
					>Save Changes</Button
				>
			</Dialog.Footer>
		</form>
	</Dialog.Content>
</Dialog.Root>

<AlertDialog.Root bind:open={showDeleteConfirm}>
	<AlertDialog.Content>
		<AlertDialog.Header>
			<AlertDialog.Title>Delete Sensor</AlertDialog.Title>
			<AlertDialog.Description>
				Are you sure you want to delete "{sensor.name}"? This action cannot be undone.
			</AlertDialog.Description>
		</AlertDialog.Header>
		<AlertDialog.Footer>
			<AlertDialog.Cancel>Cancel</AlertDialog.Cancel>
			<AlertDialog.Action onclick={handleDelete}>Delete</AlertDialog.Action>
		</AlertDialog.Footer>
	</AlertDialog.Content>
</AlertDialog.Root>
