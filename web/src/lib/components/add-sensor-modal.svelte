<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as Select from "$lib/components/ui/select/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { addSensor } from "$lib/stores/sensors.svelte";
	import type { Sensor } from "$lib/types";
	import PlusIcon from "@lucide/svelte/icons/plus";

	let open = $state(false);
	let submitted = $state(false);
	let name = $state("");
	let hardwareType = $state<Sensor["hardwareType"]>("sht41");
	let sensorType = $state<Sensor["type"]>("temperature");
	let address = $state("");

	const hardwareOptions: { value: Sensor["hardwareType"]; label: string; types: Sensor["type"][] }[] = [
		{ value: "sht41", label: "SHT41 (Temp + Humidity)", types: ["temperature", "humidity"] },
		{ value: "scd40", label: "SCD40 (CO₂ + Temp + Humidity)", types: ["co2", "temperature", "humidity"] },
		{ value: "as7341", label: "AS7341 (Light Spectrum)", types: ["light"] },
		{ value: "soil_capacitive", label: "Capacitive Soil Moisture", types: ["soil_moisture"] },
		{ value: "calculated", label: "Calculated (VPD)", types: ["vpd"] },
	];

	const sensorTypeOptions: { value: Sensor["type"]; label: string; unit: string }[] = [
		{ value: "temperature", label: "Temperature", unit: "°C" },
		{ value: "humidity", label: "Humidity", unit: "%" },
		{ value: "co2", label: "CO₂", unit: "ppm" },
		{ value: "light", label: "Light", unit: "PPFD" },
		{ value: "soil_moisture", label: "Soil Moisture", unit: "%" },
		{ value: "vpd", label: "VPD", unit: "kPa" },
	];

	const availableSensorTypes = $derived(
		hardwareOptions.find((o) => o.value === hardwareType)?.types ?? []
	);

	const filteredSensorTypeOptions = $derived(
		sensorTypeOptions.filter((opt) => availableSensorTypes.includes(opt.value))
	);

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
		const typeOption = sensorTypeOptions.find((o) => o.value === sensorType);
		const sensor: Sensor = {
			id: `sensor-${Date.now()}`,
			name,
			type: sensorType,
			unit: typeOption?.unit ?? "",
			hardwareType,
			address: address || undefined,
		};
		addSensor(sensor);
		resetForm();
		open = false;
	}

	function resetForm() {
		submitted = false;
		name = "";
		hardwareType = "sht41";
		sensorType = "temperature";
		address = "";
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
				<Label for="address">Address (optional)</Label>
				<Input id="address" bind:value={address} placeholder="e.g. 0x44 or GPIO34" />
			</div>
			<Dialog.Footer>
				<Button type="submit" disabled={!name}>Add Sensor</Button>
			</Dialog.Footer>
		</form>
	</Dialog.Content>
</Dialog.Root>
