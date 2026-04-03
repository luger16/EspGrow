<script lang="ts">
	import * as Chart from "$lib/components/ui/chart/index.js";
	import * as Card from "$lib/components/ui/card/index.js";
	import * as Checkbox from "$lib/components/ui/checkbox/index.js";
	import * as Tabs from "$lib/components/ui/tabs/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { scaleUtc } from "d3-scale";
	import { LineChart, Area, Spline } from "layerchart";
	import { curveMonotoneX, curveStepAfter } from "d3-shape";
	import DownloadIcon from "@lucide/svelte/icons/download";
	import type { Sensor, DeviceType } from "$lib/types";
	import { sensors, sensorReadings, requestHistory, getSensorHistory } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import { settings, convertTemperature, formatTimeFromDate } from "$lib/stores/settings.svelte";
	import { formatUnit, cn } from "$lib/utils";

	const SENSOR_TYPE_COLORS: Record<string, string> = {
		temperature: "hsl(0, 72%, 51%)",
		humidity: "hsl(199, 89%, 48%)",
		co2: "hsl(220, 15%, 55%)",
		light: "hsl(45, 93%, 47%)",
		vpd: "hsl(280, 65%, 60%)",
		dewpoint: "hsl(175, 70%, 45%)",
	};

	function getSensorColor(sensor: Sensor): string {
		return SENSOR_TYPE_COLORS[sensor.type] ?? "hsl(0, 0%, 50%)";
	}

	const DEVICE_TYPE_COLORS: Record<DeviceType, string> = {
		fan: "hsl(145, 45%, 55%)",
		light: "hsl(38, 70%, 55%)",
		heater: "hsl(8, 65%, 58%)",
		pump: "hsl(230, 45%, 60%)",
		humidifier: "hsl(185, 40%, 55%)",
		dehumidifier: "hsl(310, 40%, 58%)",
	};

	function getDeviceColor(device: { type: DeviceType }): string {
		return DEVICE_TYPE_COLORS[device.type] ?? "hsl(0, 0%, 60%)";
	}

	type TimeRange = "6h" | "24h" | "7d";

	let timeRange = $state<TimeRange>("6h");
	let hiddenSensors = $state<Set<string>>(new Set());
	let hiddenDevices = $state<Set<string>>(new Set());

	const timeRanges: { value: TimeRange; label: string }[] = [
		{ value: "6h", label: "6h" },
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
		device: [0, 1],
	};

	const sensorTypeLabels: Record<string, string> = {
		temperature: "Temperature",
		humidity: "Humidity",
		co2: "CO₂",
		light: "Light",
		vpd: "VPD",
		dewpoint: "Dew Point",
	};

	const gapThresholds: Record<TimeRange, number> = {
		"6h": 4 * 60 * 1000,
		"24h": 15 * 60 * 1000,
		"7d": 90 * 60 * 1000,
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

	function toggleDeviceVisibility(deviceId: string): void {
		const newSet = new Set(hiddenDevices);
		if (newSet.has(deviceId)) {
			newSet.delete(deviceId);
		} else {
			newSet.add(deviceId);
		}
		hiddenDevices = newSet;
	}

	const sensorIds = $derived(sensors.map((s) => s.id).join(","));
	const onlineDeviceIds = $derived(
		devices.filter((d) => d.isOnline !== false).map((d) => d.id).join(",")
	);

	$effect(() => {
		const sIds = sensorIds.split(",").filter(Boolean);
		const dIds = onlineDeviceIds.split(",").filter(Boolean);
		for (const id of sIds) requestHistory(id, timeRange);
		for (const id of dIds) requestHistory(id, timeRange);
		if (sIds.length === 0 && dIds.length === 0) return;
		const interval = setInterval(() => {
			for (const id of sIds) requestHistory(id, timeRange, true);
			for (const id of dIds) requestHistory(id, timeRange, true);
		}, 2 * 60 * 1000);
		return () => clearInterval(interval);
	});

	const visibleSensors = $derived(sensors.filter((s) => !hiddenSensors.has(s.id)));
	const visibleDevices = $derived(
		devices
			.filter((d) => d.isOnline !== false)
			.filter((d) => !hiddenDevices.has(d.id))
	);
	const deviceIds = $derived(new Set(visibleDevices.map((d) => d.id)));

	const sortedEntries = $derived.by(() => {
		if (visibleSensors.length === 0 && visibleDevices.length === 0) return [];

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

		for (const device of visibleDevices) {
			const history = getSensorHistory(device.id, timeRange);
			if (history.length < 2) continue;
			for (let i = 1; i < history.length; i++) {
				const timeDiff = history[i].date.getTime() - history[i - 1].date.getTime();
				if (timeDiff > threshold) {
					const gapMid = Math.floor((history[i - 1].date.getTime() + history[i].date.getTime()) / 2);
					gapTimestamps.add(gapMid);
				}
			}
		}

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

		for (const device of visibleDevices) {
			const history = getSensorHistory(device.id, timeRange);
			const [normMin, normMax] = normalizationRanges["device"];
			for (const point of history) {
				const ts = point.date.getTime();
				if (!timestampMap.has(ts)) {
					timestampMap.set(ts, {});
				}
				const normalized = ((point.value - normMin) / (normMax - normMin)) * 100;
				timestampMap.get(ts)![device.id] = {
					normalized: Math.max(0, Math.min(100, normalized)),
					raw: point.value,
				};
			}
		}

		for (const gapTs of gapTimestamps) {
			const gapEntry: Record<string, null> = {};
			for (const sensor of visibleSensors) {
				gapEntry[sensor.id] = null;
			}
			for (const device of visibleDevices) {
				gapEntry[device.id] = null;
			}
			timestampMap.set(gapTs, gapEntry);
		}

		return Array.from(timestampMap.entries()).sort((a, b) => a[0] - b[0]);
	});

	const chartData = $derived(
		sortedEntries.map(([ts, values]) => {
			const entry: Record<string, number | null | Date> = { date: new Date(ts) };
			for (const [id, data] of Object.entries(values)) {
				entry[id] = data ? data.normalized : null;
			}
			return entry;
		})
	);

	const rawValueLookup = $derived(new Map(sortedEntries.map(([ts, values]) => [ts, values])));

	function getRawValueAtTimestamp(id: string, timestamp: number): number | null {
		const values = rawValueLookup.get(timestamp);
		if (!values) return null;
		const data = values[id];
		return data?.raw ?? null;
	}

	const lightSensorIds = $derived(new Set(visibleSensors.filter((s) => s.type === "light").map((s) => s.id)));

	const series = $derived([
		...visibleSensors.map((sensor) => ({
			key: sensor.id,
			label: sensor.name,
			color: getSensorColor(sensor),
		})),
		...visibleDevices.map((device) => ({
			key: device.id,
			label: device.name,
			color: getDeviceColor(device),
		})),
	]);

	const yDomain = $derived.by(() => {
		return [0, 100];
	});

	const yTickInfo = $derived({
		ticks: [0, 25, 50, 75, 100],
		precision: 0,
	});

	const xTicks = $derived.by(() => {
		if (chartData.length < 2) return [];
		const first = (chartData[0].date as Date).getTime();
		const last = (chartData[chartData.length - 1].date as Date).getTime();
		const count = timeRange === "6h" ? 7 : timeRange === "24h" ? 6 : 7;
		const step = (last - first) / (count - 1);
		return Array.from({ length: count }, (_, i) => new Date(first + step * i));
	});

	const hasData = $derived((visibleSensors.length > 0 || visibleDevices.length > 0) && chartData.length >= 2);
	const hasFullInterval = $derived.by(() => {
		if (!hasData) return false;
		const now = new Date();
		const oldestTimestamp = (chartData[0]?.date as Date)?.getTime() ?? now.getTime();
		const intervalHours = timeRange === "6h" ? 6 : timeRange === "24h" ? 24 : 168;
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
		for (const device of visibleDevices) {
			config[device.id] = {
				label: device.name,
				color: getDeviceColor(device),
			};
		}
		return config;
	});

	function downloadCsv(): void {
		const allSensors = sensors;
		const allDevices = devices.filter(d => d.isOnline !== false);
		if (allSensors.length === 0 && allDevices.length === 0) return;

		const timestampMap = new Map<number, Record<string, number | string | null>>();

		for (const sensor of allSensors) {
			const history = getSensorHistory(sensor.id, timeRange);
			for (const point of history) {
				const ts = point.date.getTime();
				if (!timestampMap.has(ts)) {
					timestampMap.set(ts, {});
				}
				const value =
					sensor.type === "temperature" || sensor.type === "dewpoint"
						? convertTemperature(point.value, settings.temperatureUnit)
						: point.value;
				timestampMap.get(ts)![sensor.id] = Math.round(value * 10) / 10;
			}
		}

		for (const device of allDevices) {
			const history = getSensorHistory(device.id, timeRange);
			for (const point of history) {
				const ts = point.date.getTime();
				if (!timestampMap.has(ts)) {
					timestampMap.set(ts, {});
				}
				timestampMap.get(ts)![device.id] = point.value >= 0.5 ? "ON" : "OFF";
			}
		}

		if (timestampMap.size === 0) return;

		const sorted = Array.from(timestampMap.entries()).sort((a, b) => a[0] - b[0]);

		const tempUnit = settings.temperatureUnit === "fahrenheit" ? "°F" : "°C";
		const sensorHeaders = allSensors.map((sensor) => {
			if (sensor.type === "temperature" || sensor.type === "dewpoint") {
				return `${sensor.name} (${tempUnit})`;
			}
			return `${sensor.name} (${sensor.unit})`;
		});
		const deviceHeaders = allDevices.map((device) => `${device.name} (ON/OFF)`);

		const header = ["Timestamp", ...sensorHeaders, ...deviceHeaders].join(",");

		const rows = sorted.map(([ts, values]) => {
			const date = new Date(ts).toISOString();
			const sensorValues = allSensors.map((sensor) => {
				const val = values[sensor.id];
				return val !== null && val !== undefined ? String(val) : "";
			});
			const deviceValues = allDevices.map((device) => {
				const val = values[device.id];
				return val !== null && val !== undefined ? String(val) : "";
			});
			return [date, ...sensorValues, ...deviceValues].join(",");
		});

		const csv = [header, ...rows].join("\n");

		const blob = new Blob([csv], { type: "text/csv;charset=utf-8;" });
		const url = URL.createObjectURL(blob);
		const link = document.createElement("a");
		link.href = url;
		link.download = `espgrow-${timeRange}-${new Date().toISOString().slice(0, 10)}.csv`;
		link.click();
		URL.revokeObjectURL(url);
	}
</script>

<Card.Root>
	<Card.Header>
		<Card.Title>Analytics</Card.Title>
		<Card.Action>
			<div class="flex items-center gap-2">
				<Tabs.Root bind:value={timeRange}>
					<Tabs.List>
						{#each timeRanges as range}
							<Tabs.Trigger value={range.value}>{range.label}</Tabs.Trigger>
						{/each}
					</Tabs.List>
				</Tabs.Root>
				<Button variant="ghost" size="icon" class="size-8" onclick={downloadCsv} title="Export CSV">
					<DownloadIcon class="size-4" />
				</Button>
			</div>
		</Card.Action>
	</Card.Header>
	<Card.Content>
	{#if sensors.length === 0 && devices.length === 0}
		<div class="text-muted-foreground flex h-64 w-full items-center justify-center rounded-lg border border-dashed text-sm">
			No sensors or devices configured
		</div>
	{:else}
		<div class="flex gap-4">
			<div class="min-w-0 flex-1">
				{#if !hasData}
					<div class="text-muted-foreground flex h-64 w-full items-center justify-center rounded-lg border border-dashed text-sm sm:h-80">
						{#if visibleSensors.length === 0 && visibleDevices.length === 0}
							Select at least one sensor or device to view analytics
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
									{#snippet marks({ visibleSeries, getSplineProps })}
										{#each visibleSeries as s, i (s.key)}
											{#if lightSensorIds.has(s.key)}
												<Area
													y1={s.value ?? s.key}
													fill={s.color}
													fillOpacity={0.15}
													line={{ stroke: s.color, class: "stroke-2", curve: curveMonotoneX }}
													curve={curveMonotoneX}
												/>
											{:else if deviceIds.has(s.key)}
												<Spline {...getSplineProps(s, i)} curve={curveStepAfter} />
											{:else}
												<Spline {...getSplineProps(s, i)} />
											{/if}
										{/each}
									{/snippet}
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
												{@const device = visibleDevices.find(d => d.name === args.name)}
												{@const itemColor = sensor ? getSensorColor(sensor) : device ? getDeviceColor(device) : (args.item.color ?? 'currentColor')}
												{@const timestamp = args.item.payload?.date instanceof Date ? args.item.payload.date.getTime() : 0}
												{@const rawValue = sensor ? getRawValueAtTimestamp(sensor.id, timestamp) : device ? getRawValueAtTimestamp(device.id, timestamp) : null}
												<div
													class="shrink-0 rounded-[2px] border bg-[var(--indicator-color)]"
													style="--indicator-color: {itemColor}; border-color: {itemColor}; width: 4px;"
												></div>
												<div class="flex flex-1 items-center justify-between gap-2 leading-none">
													<span class="text-muted-foreground">
														{args.name}
													</span>
													<span class="text-foreground font-mono font-medium tabular-nums">
														{#if device && rawValue !== null}
															{rawValue >= 0.5 ? "ON" : "OFF"}
														{:else if sensor && rawValue !== null}
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
					{#if sensors.length > 0}
						<span class="text-muted-foreground px-1 text-xs">Sensors</span>
					{/if}
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
					{#if devices.filter(d => d.isOnline !== false).length > 0}
						<span class="text-muted-foreground mt-2 px-1 text-xs">Devices</span>
						{#each devices.filter(d => d.isOnline !== false) as device (device.id)}
							<button
								onclick={() => toggleDeviceVisibility(device.id)}
								class={cn(
									"rounded-lg border px-3 py-1.5 text-sm transition-colors",
									"hover:bg-muted",
									hiddenDevices.has(device.id)
										? "border-muted bg-muted/30 text-muted-foreground opacity-50"
										: "border-border bg-background text-foreground"
								)}
							>
								<div class="flex items-center gap-2">
									<div class="h-4 w-1 shrink-0 rounded-[2px]" style="background-color: {getDeviceColor(device)}"></div>
									<span class="truncate">{device.name}</span>
									<Checkbox.Root
										class="pointer-events-none ml-auto"
										checked={!hiddenDevices.has(device.id)}
									/>
								</div>
							</button>
						{/each}
					{/if}
				</div>
			</div>
		{/if}
	</Card.Content>
</Card.Root>
