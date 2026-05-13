<script lang="ts" module>
	import type { Sensor } from "$lib/types";

	export const sensorTypeOptions: { value: Sensor["type"]; label: string; unit: string }[] = [
		{ value: "temperature", label: "Temperature", unit: "°C" },
		{ value: "humidity", label: "Humidity", unit: "%" },
		{ value: "co2", label: "CO₂", unit: "ppm" },
		{ value: "light", label: "Light", unit: "PPFD" },
		{ value: "vpd", label: "VPD", unit: "kPa" },
		{ value: "dewpoint", label: "Dew Point", unit: "°C" },
	];

	export const hardwareOptions: {
		value: Sensor["hardwareType"];
		label: string;
		types: Sensor["type"][];
	}[] = [
		{ value: "sht3x", label: "SHT3x (Temp + Humidity)", types: ["temperature", "humidity"] },
		{ value: "sht4x", label: "SHT4x (Temp + Humidity)", types: ["temperature", "humidity"] },
		{
			value: "scd4x",
			label: "SCD4x (CO₂ + Temp + Humidity)",
			types: ["co2", "temperature", "humidity"],
		},
		{ value: "as7341", label: "AS7341 (Light Spectrum)", types: ["light"] },
		{ value: "calculated", label: "Calculated (VPD, Dew Point)", types: ["vpd", "dewpoint"] },
	];
</script>

<script lang="ts">
	import * as Select from "$lib/components/ui/select/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { sensors } from "$lib/stores/sensors.svelte";

	type Props = {
		name: string;
		hardwareType: Sensor["hardwareType"];
		sensorType: Sensor["type"];
		address: string;
		tempSourceId: string;
		humSourceId: string;
		leafTempOffset: number;
		submitted: boolean;
	};
	let {
		name = $bindable(),
		hardwareType = $bindable(),
		sensorType = $bindable(),
		address = $bindable(),
		tempSourceId = $bindable(),
		humSourceId = $bindable(),
		leafTempOffset = $bindable(),
		submitted,
	}: Props = $props();

	const availableSensorTypes = $derived(
		hardwareOptions.find((o) => o.value === hardwareType)?.types ?? []
	);
	const filteredSensorTypeOptions = $derived(
		sensorTypeOptions.filter((opt) => availableSensorTypes.includes(opt.value))
	);
	const needsSources = $derived(
		hardwareType === "calculated" && (sensorType === "vpd" || sensorType === "dewpoint")
	);
	const isVpd = $derived(hardwareType === "calculated" && sensorType === "vpd");
	const tempSensors = $derived(sensors.filter((s) => s.type === "temperature"));
	const humSensors = $derived(sensors.filter((s) => s.type === "humidity"));

	function handleHardwareChange(value: string | undefined) {
		if (!value) return;
		hardwareType = value as Sensor["hardwareType"];
		const option = hardwareOptions.find((o) => o.value === value);
		if (option && option.types.length > 0) {
			sensorType = option.types[0];
		}
	}
</script>

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
	<Select.Root
		type="single"
		value={sensorType}
		onValueChange={(v) => v && (sensorType = v as Sensor["type"])}
	>
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
{#if needsSources}
	<div class="grid gap-2">
		<Label>Temperature Source</Label>
		<Select.Root
			type="single"
			value={tempSourceId}
			onValueChange={(v) => v && (tempSourceId = v)}
		>
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
			<p class="text-destructive text-xs">Temperature source is required</p>
		{/if}
	</div>
	<div class="grid gap-2">
		<Label>Humidity Source</Label>
		<Select.Root
			type="single"
			value={humSourceId}
			onValueChange={(v) => v && (humSourceId = v)}
		>
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
			<p class="text-destructive text-xs">Humidity source is required</p>
		{/if}
	</div>
{/if}
{#if isVpd}
	<div class="grid gap-2">
		<Label for="leafOffset">Leaf Temperature Offset (°C)</Label>
		<Input
			id="leafOffset"
			type="number"
			bind:value={leafTempOffset}
			min={0}
			max={10}
			step={0.5}
		/>
		<p class="text-muted-foreground text-xs">
			How much cooler leaves are than air. Typical: 2°C.
		</p>
	</div>
{/if}
