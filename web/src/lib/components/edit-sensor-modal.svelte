<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as AlertDialog from "$lib/components/ui/alert-dialog/index.js";
	import * as Select from "$lib/components/ui/select/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { updateSensor, removeSensor, sensors } from "$lib/stores/sensors.svelte";
	import type { Sensor } from "$lib/types";

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
	let showDeleteConfirm = $state(false);

	const hardwareOptions: { value: Sensor["hardwareType"]; label: string; types: Sensor["type"][] }[] = [
		{ value: "sht3x", label: "SHT3x (Temp + Humidity)", types: ["temperature", "humidity"] },
		{ value: "sht4x", label: "SHT4x (Temp + Humidity)", types: ["temperature", "humidity"] },
		{ value: "scd4x", label: "SCD4x (CO₂ + Temp + Humidity)", types: ["co2", "temperature", "humidity"] },
		{ value: "as7341", label: "AS7341 (Light Spectrum)", types: ["light"] },
		{ value: "calculated", label: "Calculated (VPD)", types: ["vpd"] },
	];

	const sensorTypeOptions: { value: Sensor["type"]; label: string; unit: string }[] = [
		{ value: "temperature", label: "Temperature", unit: "°C" },
		{ value: "humidity", label: "Humidity", unit: "%" },
		{ value: "co2", label: "CO₂", unit: "ppm" },
		{ value: "light", label: "Light", unit: "PPFD" },
		{ value: "vpd", label: "VPD", unit: "kPa" },
	];

	const availableSensorTypes = $derived(
		hardwareOptions.find((o) => o.value === hardwareType)?.types ?? []
	);

	const filteredSensorTypeOptions = $derived(
		sensorTypeOptions.filter((opt) => availableSensorTypes.includes(opt.value))
	);

	const isVpd = $derived(hardwareType === "calculated" && sensorType === "vpd");

	const tempSensors = $derived(sensors.filter((s) => s.type === "temperature"));
	const humSensors = $derived(sensors.filter((s) => s.type === "humidity"));

	$effect(() => {
		if (open) {
			submitted = false;
			name = sensor.name;
			hardwareType = sensor.hardwareType;
			sensorType = sensor.type;
			address = sensor.address ?? "";
			tempSourceId = sensor.tempSourceId ?? "";
			humSourceId = sensor.humSourceId ?? "";
		}
	});

	function handleHardwareChange(value: string | undefined) {
		if (!value) return;
		hardwareType = value as Sensor["hardwareType"];
		const option = hardwareOptions.find((o) => o.value === value);
		if (option && option.types.length > 0) {
			// Set to first available sensor type for this hardware
			sensorType = option.types[0];
		}
	}

	function handleSubmit() {
		submitted = true;
		if (!name) return;
		if (isVpd && (!tempSourceId || !humSourceId)) return;
		const typeOption = sensorTypeOptions.find((o) => o.value === sensorType);
		updateSensor(sensor.id, {
			name,
			type: sensorType,
			unit: typeOption?.unit ?? "",
			hardwareType,
			address: address || undefined,
			tempSourceId: isVpd ? tempSourceId : undefined,
			humSourceId: isVpd ? humSourceId : undefined,
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
			<div class="grid gap-2">
				<Label for="name">Name</Label>
				<Input id="name" bind:value={name} placeholder="e.g. Tent Temperature" required />
				{#if submitted && !name}
					<p class="text-destructive text-xs">Name is required</p>
				{/if}
			</div>
			<div class="grid gap-2">
				<Label>Hardware Type</Label>
				<Select.Root type="single" value={hardwareType} onValueChange={handleHardwareChange}>
					<Select.Trigger>
						<span>{hardwareOptions.find((o) => o.value === hardwareType)?.label}</span>
					</Select.Trigger>
					<Select.Content>
						{#each hardwareOptions as option (option.value)}
							<Select.Item value={option.value}>{option.label}</Select.Item>
						{/each}
					</Select.Content>
				</Select.Root>
			</div>
			<div class="grid gap-2">
				<Label>Sensor Type</Label>
				<Select.Root type="single" value={sensorType} onValueChange={(v) => v && (sensorType = v as Sensor["type"])}>
					<Select.Trigger>
						<span>{sensorTypeOptions.find((o) => o.value === sensorType)?.label}</span>
					</Select.Trigger>
					<Select.Content>
						{#each filteredSensorTypeOptions as option (option.value)}
							<Select.Item value={option.value}>{option.label}</Select.Item>
						{/each}
					</Select.Content>
				</Select.Root>
			</div>
			<div class="grid gap-2">
				<Label for="address">I²C Address (optional)</Label>
				<Input id="address" bind:value={address} placeholder="e.g. 0x44" />
			</div>
			{#if isVpd}
				<div class="grid gap-2">
					<Label>Temperature Source</Label>
					<Select.Root type="single" value={tempSourceId} onValueChange={(v) => v && (tempSourceId = v)}>
						<Select.Trigger>
							<span>{tempSensors.find((s) => s.id === tempSourceId)?.name ?? "Select sensor..."}</span>
						</Select.Trigger>
						<Select.Content>
							{#each tempSensors as s (s.id)}
								<Select.Item value={s.id}>{s.name}</Select.Item>
							{/each}
						</Select.Content>
					</Select.Root>
					{#if submitted && !tempSourceId}
						<p class="text-destructive text-xs">Temperature source is required for VPD</p>
					{/if}
				</div>
				<div class="grid gap-2">
					<Label>Humidity Source</Label>
					<Select.Root type="single" value={humSourceId} onValueChange={(v) => v && (humSourceId = v)}>
						<Select.Trigger>
							<span>{humSensors.find((s) => s.id === humSourceId)?.name ?? "Select sensor..."}</span>
						</Select.Trigger>
						<Select.Content>
							{#each humSensors as s (s.id)}
								<Select.Item value={s.id}>{s.name}</Select.Item>
							{/each}
						</Select.Content>
					</Select.Root>
					{#if submitted && !humSourceId}
						<p class="text-destructive text-xs">Humidity source is required for VPD</p>
					{/if}
				</div>
			{/if}
			<Dialog.Footer class="flex-col gap-2 sm:flex-row sm:justify-between">
				<Button type="button" variant="destructive" onclick={() => (showDeleteConfirm = true)}>Delete</Button>
				<Button type="submit" disabled={!name || (isVpd && (!tempSourceId || !humSourceId))}>Save Changes</Button>
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
