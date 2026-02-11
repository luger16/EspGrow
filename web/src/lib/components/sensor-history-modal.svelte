<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as Chart from "$lib/components/ui/chart/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { scaleUtc } from "d3-scale";
	import { AreaChart } from "layerchart";
	import { curveNatural } from "d3-shape";
	import type { Sensor } from "$lib/types";
	import { getSensorHistory, requestHistory } from "$lib/stores/sensors.svelte";

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

	// Check if data spans the full requested interval
	const intervalHours = $derived({ "12h": 12, "24h": 24, "7d": 168 }[timeRange]);
	const hasData = $derived(history.length >= 2);
	const hasFullInterval = $derived.by(() => {
		if (!hasData) return false;
		const now = new Date();
		const oldestTimestamp = history[0].date.getTime();
		const requiredMs = intervalHours * 60 * 60 * 1000;
		return now.getTime() - oldestTimestamp >= requiredMs;
	});

	const yDomain = $derived.by(() => {
		if (history.length === 0) return undefined;
		const values = history.map((d) => d.value);
		const min = Math.min(...values);
		const max = Math.max(...values);
		const padding = (max - min) * 0.05 || 0.5;
		return [min - padding, max + padding];
	});

	const chartConfig = $derived({
		value: { label: `${sensor.unit}`, color: "var(--chart-1)" },
	} satisfies Chart.ChartConfig);
</script>

<Dialog.Root {open} {onOpenChange}>
	<Dialog.Content class="max-w-2xl" showCloseButton={false}>
		<div class="flex items-center justify-between gap-4">
			<Dialog.Title class="text-lg font-semibold">{sensor.name}</Dialog.Title>
			<div class="flex items-center gap-1">
				{#each timeRanges as range}
					<Button
						variant={timeRange === range.value ? "secondary" : "ghost"}
						size="sm"
						onclick={() => (timeRange = range.value)}
					>
						{range.label}
					</Button>
				{/each}
				<Dialog.Close
					class="ring-offset-background focus:ring-ring ml-2 rounded-sm opacity-70 transition-opacity hover:opacity-100 focus:ring-2 focus:ring-offset-2 focus:outline-hidden disabled:pointer-events-none"
				>
					<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M18 6 6 18"/><path d="m6 6 12 12"/></svg>
					<span class="sr-only">Close</span>
				</Dialog.Close>
			</div>
		</div>
		{#if !hasData}
			<div class="text-muted-foreground flex aspect-2/1 w-full items-center justify-center text-sm">
				No data available for this time range
			</div>
		{:else}
			{#if !hasFullInterval}
				<div
					class="bg-muted text-muted-foreground mb-2 rounded-md border px-3 py-2 text-sm"
				>
					<strong>Note:</strong> Data is incomplete for this time range. Showing available data.
				</div>
			{/if}
			<Chart.ChartContainer config={chartConfig} class="aspect-2/1 w-full">
				<AreaChart
					data={history}
					x="date"
					xScale={scaleUtc()}
					y="value"
					yDomain={yDomain}
					yBaseline={undefined}
					props={{
						area: {
							curve: curveNatural,
							"fill-opacity": 0.4,
							line: { class: "stroke-1" },
						},
						xAxis: {
							ticks: timeRange === "12h" ? 6 : timeRange === "24h" ? 8 : 7,
							format: (v: Date) => {
								if (timeRange === "7d") {
									return v.toLocaleDateString("en-US", { month: "short", day: "numeric" });
								}
								return v.toLocaleTimeString("en-US", { hour: "numeric" });
							},
						},
						yAxis: {
							format: (v: number) => `${v}`,
						},
					}}
				>
					{#snippet tooltip()}
						<Chart.Tooltip
							labelFormatter={(v: Date) => {
								if (timeRange === "7d") {
									return v.toLocaleString("en-US", {
										weekday: "short",
										month: "short",
										day: "numeric",
										hour: "numeric",
										minute: "2-digit",
									});
								}
								return v.toLocaleString("en-US", {
									hour: "numeric",
									minute: "2-digit",
								});
							}}
						/>
					{/snippet}
				</AreaChart>
			</Chart.ChartContainer>
		{/if}
	</Dialog.Content>
</Dialog.Root>
