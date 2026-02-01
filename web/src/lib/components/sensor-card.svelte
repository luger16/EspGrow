<script lang="ts">
	import * as Card from "$lib/components/ui/card/index.js";
	import type { Sensor, SensorReading } from "$lib/types";
	import ThermometerIcon from "@lucide/svelte/icons/thermometer";
	import DropletIcon from "@lucide/svelte/icons/droplet";
	import CloudIcon from "@lucide/svelte/icons/cloud";
	import SunIcon from "@lucide/svelte/icons/sun";
	import LeafIcon from "@lucide/svelte/icons/leaf";
	import type { Component } from "svelte";

	let {
		sensor,
		reading,
		onclick,
	}: { sensor: Sensor; reading?: SensorReading; onclick?: () => void } = $props();

	const iconMap: Record<Sensor["type"], Component> = {
		temperature: ThermometerIcon,
		humidity: DropletIcon,
		co2: CloudIcon,
		light: SunIcon,
		soil_moisture: LeafIcon,
	};

	const Icon = $derived(iconMap[sensor.type]);
	const displayValue = $derived(reading ? `${reading.value}${sensor.unit}` : "—");
</script>

<button type="button" class="w-full text-left" {onclick}>
	<Card.Root class="py-4 transition-colors hover:bg-muted/50">
		<Card.Content class="flex flex-col gap-3 px-4">
			<div class="flex items-center justify-between gap-2">
				<span class="text-sm font-medium">{sensor.name}</span>
				<div class="flex size-8 shrink-0 items-center justify-center rounded-lg bg-muted">
					<Icon class="size-4 text-muted-foreground" />
				</div>
			</div>
			<span class="text-2xl font-semibold tabular-nums">{displayValue}</span>
		</Card.Content>
	</Card.Root>
</button>
