<script lang="ts">
	import * as Card from "$lib/components/ui/card/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import ChartNoAxesCombined from "@lucide/svelte/icons/chart-no-axes-combined";
	import type { Sensor, SensorReading } from "$lib/types";
	import { settings, formatTemperature } from "$lib/stores/settings.svelte";
	import { sensorIcons } from "$lib/icons";
	import { cn, formatUnit } from "$lib/utils";
	import { getSensorStatus, getSensorTarget } from "$lib/stores/climate.svelte";

	let {
		sensor,
		reading,
		onclick,
		onvpdclick,
	}: { sensor: Sensor; reading?: SensorReading; onclick?: () => void; onvpdclick?: () => void } = $props();

	const Icon = $derived(sensorIcons[sensor.type]);
	const displayValue = $derived.by(() => {
		if (!reading) return "—";
		if (sensor.type === "temperature" || sensor.type === "dewpoint") {
			return formatTemperature(reading.value, settings.temperatureUnit);
		}
		return `${reading.value}${formatUnit(sensor.unit)}`;
	});
	const status = $derived(getSensorStatus(sensor.id));
	const target = $derived(getSensorTarget(sensor.type));

	const targetDisplay = $derived.by(() => {
		if (target === undefined) return undefined;
		if (sensor.type === "temperature" || sensor.type === "dewpoint") {
			return formatTemperature(target, settings.temperatureUnit);
		}
		return `${target}${formatUnit(sensor.unit)}`;
	});

	const borderClass = $derived.by(() => {
		if (!status) return "";
		if (status === "optimal") return "border-l-2 border-l-green-500/60";
		if (status === "warning") return "border-l-2 border-l-amber-500/60";
		if (status === "critical") return "border-l-2 border-l-red-500/60";
		return "";
	});
</script>

<button type="button" class="w-full text-left" {onclick}>
	<Card.Root class={cn("py-2.5 transition-colors hover:bg-muted/50", borderClass)}>
		<Card.Content class="flex items-center gap-3 px-3">
			<div class="flex size-7 shrink-0 items-center justify-center rounded-md bg-muted">
				<Icon class="size-3.5 text-muted-foreground" />
			</div>
			<div class="min-w-0 flex-1">
				<p class="truncate text-xs text-muted-foreground">{sensor.name}</p>
				<div class="flex items-baseline gap-1.5">
					<span class="text-base font-semibold tabular-nums">{displayValue}</span>
					{#if targetDisplay !== undefined}
						<span class="text-[10px] text-muted-foreground/70 tabular-nums">/ {targetDisplay}</span>
					{/if}
				</div>
			</div>
			{#if sensor.type === "vpd" && onvpdclick}
				<Button
					variant="outline"
					size="icon"
					class="size-7 shrink-0"
					onclick={(e: MouseEvent) => { e.stopPropagation(); onvpdclick(); }}
				>
					<ChartNoAxesCombined class="size-3.5" />
				</Button>
			{/if}
		</Card.Content>
	</Card.Root>
</button>
