<script lang="ts">
	import * as Card from "$lib/components/ui/card/index.js";
	import type { Sensor, SensorReading } from "$lib/types";
	import { settings, formatTemperature } from "$lib/stores/settings.svelte";
	import { sensorIcons } from "$lib/icons";
	import { formatUnit } from "$lib/utils";

	let {
		sensor,
		reading,
		onclick,
	}: { sensor: Sensor; reading?: SensorReading; onclick?: () => void } = $props();

	const Icon = $derived(sensorIcons[sensor.type]);
	const displayValue = $derived.by(() => {
		if (!reading) return "—";
		if (sensor.type === "temperature") {
			return formatTemperature(reading.value, settings.temperatureUnit);
		}
		return `${reading.value}${formatUnit(sensor.unit)}`;
	});
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
