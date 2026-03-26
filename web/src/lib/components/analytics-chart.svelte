<script lang="ts">
	import * as Chart from "$lib/components/ui/chart/index.js";
	import * as Card from "$lib/components/ui/card/index.js";
	import * as Checkbox from "$lib/components/ui/checkbox/index.js";
	import * as Tabs from "$lib/components/ui/tabs/index.js";
	import { scaleUtc } from "d3-scale";
	import { LineChart } from "layerchart";
	import { curveMonotoneX } from "d3-shape";
	import type { Sensor } from "$lib/types";
	import { sensors, sensorReadings, requestHistory, getSensorHistory } from "$lib/stores/sensors.svelte";
	import { settings, convertTemperature, formatTimeFromDate } from "$lib/stores/settings.svelte";
	import { formatUnit, cn } from "$lib/utils";

	const SENSOR_TYPE_COLORS: Record<string, string> = {
		temperature: "hsl(0, 72%, 51%)",      // red — heat
		humidity: "hsl(199, 89%, 48%)",        // blue — water
		co2: "hsl(220, 15%, 55%)",             // slate — gas
		light: "hsl(45, 93%, 47%)",            // amber — sun
		vpd: "hsl(280, 65%, 60%)",             // purple — derived
		dewpoint: "hsl(175, 70%, 45%)",        // teal — condensation
	};

	function getSensorColor(sensor: Sensor): string {
		return SENSOR_TYPE_COLORS[sensor.type] ?? "hsl(0, 0%, 50%)";
	}

	type TimeRange = "24h" | "7d";

	let timeRange = $state<TimeRange>("24h");
	let hiddenSensors = $state<Set<string>>(new Set());

	const timeRanges: { value: TimeRange; label: string }[] = [
		{ value: "24h", label: "24h" },
		{ value: "7d", label: "7d" },
	];

	const normalizationRanges: Record<string, [number, number]> = {
		temperature: [10, 40],
		dewpoint: [10, 40],
		humidity: [0, 100],
		co2: [300, 2000],
		light: [0, 1500],
		vpd: [0, 2.5],
	};

	const sensorTypeLabels: Record<string, string> = {
		temperature: "Temperature",
		humidity: "Humidity",
		co2: "CO₂",
		light: "Light",
		vpd: "VPD",
		dewpoint: "Dew Point",
	};

	// Gap detection thresholds per time range (in milliseconds)
	const gapThresholds: Record<TimeRange, number> = {
		"24h": 15 * 60 * 1000,  // 15 minutes
		"7d": 90 * 60 * 1000,   // 1.5 hours
	};

	function toggleSensorVisibility(sensorId: string): void {
		const newSet = new Set(hiddenSensors);
		if (newSet.has(sensorId)) {
			newSet.delete(sensorId);
		} else {
			newSet.add(sensorId);
		}
		hiddenSensors = newSet;
	}

	$effect(() => {
		if (sensors.length > 0) {
			for (const sensor of sensors) {
				requestHistory(sensor.id, timeRange);
			}
		}
	});

	const visibleSensors = $derived(sensors.filter((s) => !hiddenSensors.has(s.id)));

	// Shared sorted entries with gap detection — inserts null markers at data gaps
	const sortedEntries = $derived.by(() => {
		if (visibleSensors.length === 0) return [];

		// First, collect all per-sensor timestamps with their gaps
		const threshold = gapThresholds[timeRange];
		const gapTimestamps = new Set<number>();

		for (const sensor of visibleSensors) {
			const history = getSensorHistory(sensor.id, timeRange);
			if (history.length < 2) continue;
			for (let i = 1; i < history.length; i++) {
				const timeDiff = history[i].date.getTime() - history[i - 1].date.getTime();
				if (timeDiff > threshold) {
					const gapMid = Math.floor((history[i - 1].date.getTime() + history[i].date.getTime()) / 2);
					gapTimestamps.add(gapMid);
				}
			}
		}

		// Build timestamp map with normalized + raw values
		const timestampMap = new Map<number, Record<string, { normalized: number; raw: number } | null>>();

		for (const sensor of visibleSensors) {
			const history = getSensorHistory(sensor.id, timeRange);
			const [normMin, normMax] = normalizationRanges[sensor.type] ?? [0, 100];
			for (const point of history) {
				const ts = point.date.getTime();
				if (!timestampMap.has(ts)) {
					timestampMap.set(ts, {});
				}
				const rawValue =
					sensor.type === "temperature" || sensor.type === "dewpoint"
						? convertTemperature(point.value, settings.temperatureUnit)
						: point.value;
				const normalized = ((rawValue - normMin) / (normMax - normMin)) * 100;
				timestampMap.get(ts)![sensor.id] = {
					normalized: Math.max(0, Math.min(100, normalized)),
					raw: rawValue,
				};
			}
		}

		// Insert gap markers (all sensor values null at gap timestamp)
		for (const gapTs of gapTimestamps) {
			const gapEntry: Record<string, null> = {};
			for (const sensor of visibleSensors) {
				gapEntry[sensor.id] = null;
			}
			timestampMap.set(gapTs, gapEntry);
		}

		return Array.from(timestampMap.entries()).sort((a, b) => a[0] - b[0]);
	});

	const chartData = $derived(
		sortedEntries.map(([ts, values]) => {
			const entry: Record<string, number | null | Date> = { date: new Date(ts) };
			for (const [sensorId, data] of Object.entries(values)) {
				entry[sensorId] = data ? data.normalized : null;
			}
			return entry;
		})
	);

	// Lookup raw values from sortedEntries by timestamp for tooltip display
	function getRawValueAtTimestamp(sensorId: string, timestamp: number): number | null {
		const entry = sortedEntries.find(([ts]) => ts === timestamp);
		if (!entry) return null;
		const sensorData = entry[1][sensorId];
		return sensorData?.raw ?? null;
	}

	const series = $derived(
		visibleSensors.map((sensor) => ({
			key: sensor.id,
			label: sensor.name,
			color: getSensorColor(sensor),
		}))
	);

	const yDomain = $derived.by(() => {
		// Always 0-100 for normalized view
		return [0, 100];
	});

	// Y-axis ticks - always 0-100 for normalized view
	const yTickInfo = $derived({
		ticks: [0, 25, 50, 75, 100],
		precision: 0,
	});

	const xTicks = $derived.by(() => {
		if (chartData.length < 2) return [];
		const first = (chartData[0].date as Date).getTime();
		const last = (chartData[chartData.length - 1].date as Date).getTime();

		// Adaptive tick count based on time range
		const count = timeRange === "24h" ? 6 : 7;

		const step = (last - first) / (count - 1);
		return Array.from({ length: count }, (_, i) => new Date(first + step * i));
	});

	const hasData = $derived(visibleSensors.length > 0 && chartData.length >= 2);
	const hasFullInterval = $derived.by(() => {
		if (!hasData) return false;
		const now = new Date();
		const oldestTimestamp = (chartData[0]?.date as Date)?.getTime() ?? now.getTime();
		const intervalHours = timeRange === "24h" ? 24 : 168;
		const requiredMs = intervalHours * 60 * 60 * 1000;
		return now.getTime() - oldestTimestamp >= requiredMs;
	});

	const chartConfig = $derived.by(() => {
		const config: Chart.ChartConfig = {};
		for (const sensor of visibleSensors) {
			config[sensor.id] = {
				label: sensor.name,
				color: getSensorColor(sensor),
			};
		}
		return config;
	});
</script>

<Card.Root>
	<Card.Header>
		<Card.Title>Analytics</Card.Title>
		<Card.Action>
			<Tabs.Root bind:value={timeRange}>
				<Tabs.List>
					{#each timeRanges as range}
						<Tabs.Trigger value={range.value}>{range.label}</Tabs.Trigger>
					{/each}
				</Tabs.List>
			</Tabs.Root>
		</Card.Action>
	</Card.Header>
	<Card.Content>
	{#if sensors.length === 0}
		<div class="text-muted-foreground flex h-64 w-full items-center justify-center rounded-lg border border-dashed text-sm">
			No sensors configured
		</div>
	{:else}
		<div class="flex gap-4">
			<div class="min-w-0 flex-1">
				{#if !hasData}
					<div class="text-muted-foreground flex h-64 w-full items-center justify-center rounded-lg border border-dashed text-sm sm:h-80">
						{#if visibleSensors.length === 0}
							Select at least one sensor to view analytics
						{:else}
							No data available for this time range
						{/if}
					</div>
				{:else}
					{#if !hasFullInterval}
						<div class="bg-muted text-muted-foreground mb-2 rounded-md border px-3 py-2 text-sm">
							<strong>Note:</strong> Data is incomplete for this time range. Showing available data.
						</div>
					{/if}
					<Chart.ChartContainer
						config={chartConfig}
						class="h-64 w-full sm:h-80 [&_.lc-axis-tick-label]:text-[10px] sm:[&_.lc-axis-tick-label]:text-xs"
					>
						{#key timeRange}
							<LineChart
									data={chartData}
									x="date"
									xScale={scaleUtc()}
									{series}
									yDomain={yDomain}
									props={{
										spline: { curve: curveMonotoneX, class: "stroke-2" },
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
											format: (v: number) =>
												v.toLocaleString(navigator.language, { maximumFractionDigits: 0 }) + "%",
										},
									grid: {
										yTicks: yTickInfo.ticks,
									},
									}}
								>
									{#snippet tooltip()}
										<Chart.Tooltip
											indicator="line"
											labelFormatter={(v: unknown) => {
												const date = v as Date;
												if (timeRange === "7d") {
													return (
														date.toLocaleString(navigator.language, {
															weekday: "short",
															month: "short",
															day: "numeric",
														}) +
														" " +
														formatTimeFromDate(date)
													);
												}
												return formatTimeFromDate(date);
											}}
										>
											{#snippet formatter(args: { value: unknown; name: string; item: { payload: Record<string, unknown>; key: string; color?: string }; index: number })}
												{@const sensor = visibleSensors.find(s => s.name === args.name)}
												{@const sensorColor = sensor ? getSensorColor(sensor) : (args.item.color ?? 'currentColor')}
												{@const timestamp = args.item.payload?.date instanceof Date ? args.item.payload.date.getTime() : 0}
												{@const rawValue = sensor ? getRawValueAtTimestamp(sensor.id, timestamp) : null}
												<div
													class="shrink-0 rounded-[2px] border bg-[var(--indicator-color)]"
													style="--indicator-color: {sensorColor}; border-color: {sensorColor}; width: 4px;"
												></div>
												<div class="flex flex-1 items-center justify-between gap-2 leading-none">
													<span class="text-muted-foreground">
														{args.name}
													</span>
													<span class="text-foreground font-mono font-medium tabular-nums">
														{#if sensor && rawValue !== null}
															{#if sensor.type === "temperature" || sensor.type === "dewpoint"}
																{rawValue.toFixed(1)}{settings.temperatureUnit === "fahrenheit" ? "°F" : "°C"}
															{:else}
																{rawValue}{formatUnit(sensor.unit)}
															{/if}
														{:else}
															--
														{/if}
													</span>
												</div>
											{/snippet}
										</Chart.Tooltip>
									{/snippet}
								</LineChart>
							{/key}
						</Chart.ChartContainer>
					{/if}
				</div>

				<div class="flex w-40 shrink-0 flex-col gap-1.5">
					{#each sensors as sensor, i (sensor.id)}
						<button
							onclick={() => toggleSensorVisibility(sensor.id)}
							class={cn(
								"rounded-lg border px-3 py-1.5 text-sm transition-colors",
								"hover:bg-muted",
								hiddenSensors.has(sensor.id)
									? "border-muted bg-muted/30 text-muted-foreground opacity-50"
									: "border-border bg-background text-foreground"
							)}
						>
							<div class="flex items-center gap-2">
								<div class="h-4 w-1 shrink-0 rounded-[2px]" style="background-color: {getSensorColor(sensor)}"></div>
								<span class="truncate">{sensor.name}</span>
								<Checkbox.Root
									class="pointer-events-none ml-auto"
									checked={!hiddenSensors.has(sensor.id)}
								/>
							</div>
						</button>
					{/each}
				</div>
			</div>
		{/if}
	</Card.Content>
</Card.Root>
