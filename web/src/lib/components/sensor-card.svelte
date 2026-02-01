<script lang="ts">
	import * as Card from "$lib/components/ui/card/index.js";
	import type { Sensor, SensorReading } from "$lib/types";
	import ThermometerIcon from "@lucide/svelte/icons/thermometer";
	import DropletIcon from "@lucide/svelte/icons/droplet";
	import CloudIcon from "@lucide/svelte/icons/cloud";
	import SunIcon from "@lucide/svelte/icons/sun";
	import LeafIcon from "@lucide/svelte/icons/leaf";
	import type { Component } from "svelte";

	let { sensor, reading }: { sensor: Sensor; reading?: SensorReading } = $props();

	const iconMap: Record<Sensor["type"], Component> = {
		temperature: ThermometerIcon,
		humidity: DropletIcon,
		co2: CloudIcon,
		light: SunIcon,
		soil_moisture: LeafIcon,
	};

	const typeLabels: Record<Sensor["type"], string> = {
		temperature: "Temperature",
		humidity: "Humidity",
		co2: "CO₂",
		light: "Light",
		soil_moisture: "Soil",
	};

	const Icon = $derived(iconMap[sensor.type]);
	const displayValue = $derived(reading ? `${reading.value}${sensor.unit}` : "—");
</script>

<Card.Root class="flex flex-col">
	<Card.Content class="flex items-center gap-4 p-4">
		<div class="bg-muted flex size-10 shrink-0 items-center justify-center rounded-lg">
			<Icon class="text-muted-foreground size-5" />
		</div>
		<div class="flex flex-1 flex-col gap-0.5">
			<span class="font-medium">{sensor.name}</span>
			<span class="text-muted-foreground text-xs">{typeLabels[sensor.type]}</span>
		</div>
		<span class="text-2xl font-semibold tabular-nums">{displayValue}</span>
	</Card.Content>
</Card.Root>
