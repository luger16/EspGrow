<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as Chart from "$lib/components/ui/chart/index.js";
	import * as Tabs from "$lib/components/ui/tabs/index.js";
	import { scaleUtc } from "d3-scale";
	import { AreaChart } from "layerchart";
	import { curveNatural } from "d3-shape";
	import type { Sensor } from "$lib/types";
	import { getSensorHistory, requestHistory } from "$lib/stores/sensors.svelte";
	import { settings, convertTemperature, formatTimeFromDate } from "$lib/stores/settings.svelte";
	import { formatUnit } from "$lib/utils";

	let {
		sensor,
		open = $bindable(false),
		onOpenChange,
	}: {
		sensor: Sensor;
		open?: boolean;
		onOpenChange?: (open: boolean) => void;
	} = $props();

	type TimeRange = "12h" | "24h" | "7d";
	let timeRange = $state<TimeRange>("24h");

	const timeRanges: { value: TimeRange; label: string }[] = [
		{ value: "12h", label: "12h" },
		{ value: "24h", label: "24h" },
		{ value: "7d", label: "7d" },
	];

	$effect(() => {
		if (open && sensor) {
			requestHistory(sensor.id, timeRange);
		}
	});

	const history = $derived(getSensorHistory(sensor.id, timeRange));

	const intervalHours = $derived({ "12h": 12, "24h": 24, "7d": 168 }[timeRange]);
	const hasData = $derived(history.length >= 2);
	const hasFullInterval = $derived.by(() => {
		if (!hasData) return false;
		const now = new Date();
		const oldestTimestamp = history[0].date.getTime();
		const requiredMs = intervalHours * 60 * 60 * 1000;
		return now.getTime() - oldestTimestamp >= requiredMs;
	});

	const chartData = $derived.by(() => {
		if (history.length < 2) {
			return history.map((point) => ({
				date: point.date,
				value: sensor.type === "temperature"
					? convertTemperature(point.value, settings.temperatureUnit)
					: point.value,
			}));
		}

		const expectedInterval = { "12h": 5 * 60 * 1000, "24h": 10 * 60 * 1000, "7d": 60 * 60 * 1000 }[timeRange];
		const gapThreshold = expectedInterval * 1.5;

		const result: { date: Date; value: number | null }[] = [];
		for (let i = 0; i < history.length; i++) {
			if (i > 0) {
				const timeDiff = history[i].date.getTime() - history[i - 1].date.getTime();
				if (timeDiff > gapThreshold) {
					const gapMid = new Date((history[i - 1].date.getTime() + history[i].date.getTime()) / 2);
					result.push({ date: gapMid, value: null });
				}
			}
			const convertedValue = sensor.type === "temperature"
				? convertTemperature(history[i].value, settings.temperatureUnit)
				: history[i].value;
			result.push({ date: history[i].date, value: convertedValue });
		}
		return result;
	});

	const yDomain = $derived.by(() => {
		const values = history.map((d) => {
			if (sensor.type === "temperature") {
				return convertTemperature(d.value, settings.temperatureUnit);
			}
			return d.value;
		});
		const min = Math.min(...values);
		const max = Math.max(...values);
		const padding = (max - min) * 0.15 || 0.5;
		return [min - padding, max + padding];
	});

	// Y-axis ticks - precision adapts to data range automatically
	const yTickInfo = $derived.by(() => {
		const [min, max] = yDomain;
		if (min === undefined || max === undefined || isNaN(min) || isNaN(max)) {
			return { ticks: [0, 50, 100], precision: 0 };
		}
		const range = max - min;
		const rawStep = range / 4;
		// Round step to 1 significant figure (e.g. 0.023 → 0.02, 173 → 200)
		const mag = Math.pow(10, Math.floor(Math.log10(rawStep)));
		const step = Math.round(rawStep / mag) * mag;
		const precision = Math.max(0, Math.ceil(-Math.log10(step)));
		const factor = Math.pow(10, precision);
		const start = Math.ceil(min * factor) / factor;
		const ticks: number[] = [];
		for (let v = start; v <= max + step * 0.01; v += step) {
			ticks.push(Math.round(v * factor) / factor);
		}
		return { ticks, precision };
	});

	const xTicks = $derived.by(() => {
		if (chartData.length < 2) return [];
		const first = chartData[0].date.getTime();
		const last = chartData[chartData.length - 1].date.getTime();
		const count = 3;
		const step = (last - first) / (count - 1);
		return Array.from({ length: count }, (_, i) => new Date(first + step * i));
	});

	const sensorTypeLabels: Record<string, string> = {
		temperature: "Temperature",
		humidity: "Humidity",
		co2: "CO₂",
		light: "Light",
		vpd: "VPD",
	};

	const chartConfig = $derived({
		value: {
			label: sensorTypeLabels[sensor.type] || sensor.type,
			color: "var(--primary)",
		},
	} satisfies Chart.ChartConfig);

	const series = $derived([
		{
			key: "value" as const,
			label: chartConfig.value.label,
			color: chartConfig.value.color,
		},
	]);
</script>

<Dialog.Root {open} {onOpenChange}>
	<Dialog.Content class="w-full max-w-[calc(100%-1rem)] gap-0 overflow-hidden p-0 sm:max-w-2xl" showCloseButton={false}>
		<div class="flex items-start justify-between gap-2 px-4 pt-4 sm:px-6 sm:pt-6">
			<div class="min-w-0 flex-1">
				<Dialog.Title class="text-lg font-semibold leading-tight">{sensor.name}</Dialog.Title>
			</div>
			<div class="flex items-center gap-1 shrink-0">
				<Tabs.Root bind:value={timeRange}>
					<Tabs.List>
						{#each timeRanges as range}
							<Tabs.Trigger value={range.value}>{range.label}</Tabs.Trigger>
						{/each}
					</Tabs.List>
				</Tabs.Root>
				<Dialog.Close
					class="ring-offset-background focus:ring-ring ml-2 rounded-sm opacity-70 transition-opacity hover:opacity-100 focus:ring-2 focus:ring-offset-2 focus:outline-hidden disabled:pointer-events-none"
				>
					<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M18 6 6 18"/><path d="m6 6 12 12"/></svg>
					<span class="sr-only">Close</span>
				</Dialog.Close>
			</div>
		</div>
		{#if !hasData}
			<div class="text-muted-foreground flex h-64 w-full items-center justify-center px-4 pb-4 text-sm sm:h-80 sm:px-6 sm:pb-6">
				No data available for this time range
			</div>
		{:else}
			{#if !hasFullInterval}
				<div class="bg-muted text-muted-foreground mx-4 mt-4 rounded-md border px-3 py-2 text-sm wrap-break-word sm:mx-6">
					<strong>Note:</strong> Data is incomplete for this time range. Showing available data.
				</div>
			{/if}
			<Chart.ChartContainer config={chartConfig} class="min-w-0 h-64 w-full px-6 pb-4 pt-4 sm:h-80 sm:px-8 sm:pb-6 [&_.lc-axis-tick-label]:text-[10px] sm:[&_.lc-axis-tick-label]:text-xs">
				{#key timeRange}
					<AreaChart
						data={chartData}
						x="date"
						xScale={scaleUtc()}
						{series}
						yDomain={yDomain}
						props={{
							area: {
								curve: curveNatural,
								"fill-opacity": 0.4,
								defined: (d: { value: number | null }) => d.value !== null,
								line: { class: "stroke-1" },
								motion: "tween",
							},
							xAxis: {
								ticks: xTicks,
								format: (v: Date) => {
									if (timeRange === "7d") {
										return v.toLocaleDateString(navigator.language, { month: "short", day: "numeric" });
									}
									return formatTimeFromDate(v);
								},
							},
						yAxis: {
							ticks: yTickInfo.ticks,
							format: (v: number) => v.toLocaleString(navigator.language, { maximumFractionDigits: yTickInfo.precision }),
						},
						}}
					>
						{#snippet tooltip()}
							<Chart.Tooltip
								labelFormatter={(v: unknown) => {
									const date = v as Date;
									if (timeRange === "7d") {
										return date.toLocaleString(navigator.language, {
											weekday: "short",
											month: "short",
											day: "numeric",
										}) + " " + formatTimeFromDate(date);
									}
									return formatTimeFromDate(date);
								}}
							>
								{#snippet formatter({ value }: { value: unknown })}
									<div class="flex w-full items-center justify-between gap-2">
										<span class="text-muted-foreground">{sensorTypeLabels[sensor.type] || sensor.type}</span>
										<span class="text-foreground font-mono font-medium tabular-nums">
											{#if sensor.type === "temperature"}
												{typeof value === "number" ? value.toFixed(1) : value}{settings.temperatureUnit === "fahrenheit" ? "°F" : "°C"}
											{:else}
												{value}{formatUnit(sensor.unit)}
											{/if}
										</span>
									</div>
								{/snippet}
							</Chart.Tooltip>
						{/snippet}
					</AreaChart>
				{/key}
			</Chart.ChartContainer>
		{/if}
	</Dialog.Content>
</Dialog.Root>
