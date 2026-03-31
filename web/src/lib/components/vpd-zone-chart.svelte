<script lang="ts">
	import { Chart, Svg, Axis } from "layerchart";
	import * as ChartUI from "$lib/components/ui/chart/index.js";
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import { scaleLinear } from "d3-scale";
	import { sensors, sensorReadings } from "$lib/stores/sensors.svelte";
	import { settings, convertTemperature } from "$lib/stores/settings.svelte";
	import { getIsDay, getCurrentTargets, WARNING_MARGIN } from "$lib/stores/climate.svelte";
	import type { Sensor } from "$lib/types";

	const TEMP_MIN_C = 15;
	const TEMP_MAX_C = 35;
	const HUM_MIN = 20;
	const HUM_MAX = 100;
	const TEMP_STEP = 1;
	const HUM_STEP = 2;

	const ZONE_COLORS = {
		critical: "hsl(0 72% 51% / 0.5)",
		warning: "hsl(25 95% 53% / 0.5)",
		optimal: "hsl(142 71% 55% / 0.5)",
	};

	function calculateVPD(tempCelsius: number, humidity: number): number {
		const svp = 0.6108 * Math.exp((17.27 * tempCelsius) / (tempCelsius + 237.3));
		return svp * (1 - humidity / 100);
	}

	const targets = $derived(getCurrentTargets());
	const isDay = $derived(getIsDay());
	const targetVpd = $derived(isDay ? targets.vpd.day : targets.vpd.night);
	const margin = WARNING_MARGIN.vpd;

	const vpdZones = $derived.by(() => {
		const t = targetVpd;
		return [
			{ max: t - margin * 2, color: ZONE_COLORS.critical, label: "Critical Low" },
			{ max: t - margin, color: ZONE_COLORS.warning, label: "Low" },
			{ max: t + margin, color: ZONE_COLORS.optimal, label: "Optimal" },
			{ max: t + margin * 2, color: ZONE_COLORS.warning, label: "High" },
			{ max: Infinity, color: ZONE_COLORS.critical, label: "Critical High" },
		];
	});

	function getZoneColor(vpd: number): string {
		for (const zone of vpdZones) {
			if (vpd < zone.max) return zone.color;
		}
		return ZONE_COLORS.critical;
	}

	const tempSensor = $derived(sensors.find((s: Sensor) => s.type === "temperature"));
	const humSensor = $derived(sensors.find((s: Sensor) => s.type === "humidity"));
	const currentTemp = $derived(tempSensor ? sensorReadings[tempSensor.id]?.value : undefined);
	const currentHum = $derived(humSensor ? sensorReadings[humSensor.id]?.value : undefined);
	const hasRequiredSensors = $derived(!!tempSensor && !!humSensor);

	const currentVpd = $derived(
		currentTemp !== undefined && currentHum !== undefined
			? calculateVPD(currentTemp, currentHum)
			: undefined
	);

	const zoneRects = $derived.by(() => {
		const rects: { tempMin: number; tempMax: number; humMin: number; humMax: number; color: string }[] = [];
		for (let temp = TEMP_MIN_C; temp < TEMP_MAX_C; temp += TEMP_STEP) {
			for (let hum = HUM_MIN; hum < HUM_MAX; hum += HUM_STEP) {
				const vpd = calculateVPD(temp + TEMP_STEP / 2, hum + HUM_STEP / 2);
				rects.push({
					tempMin: temp,
					tempMax: temp + TEMP_STEP,
					humMin: hum,
					humMax: hum + HUM_STEP,
					color: getZoneColor(vpd),
				});
			}
		}
		return rects;
	});

	const tempUnit = $derived(settings.temperatureUnit);
	const tempMin = $derived(convertTemperature(TEMP_MIN_C, tempUnit));
	const tempMax = $derived(convertTemperature(TEMP_MAX_C, tempUnit));

	const tempTicks = $derived.by(() => {
		const ticks: number[] = [];
		for (let t = TEMP_MIN_C; t <= TEMP_MAX_C; t += 5) {
			ticks.push(convertTemperature(t, tempUnit));
		}
		return ticks;
	});

	const humTicks = $derived.by(() => {
		const ticks: number[] = [];
		for (let h = HUM_MIN; h <= HUM_MAX; h += 20) {
			ticks.push(h);
		}
		return ticks;
	});

	const currentTempDisplay = $derived(
		currentTemp !== undefined ? convertTemperature(currentTemp, tempUnit) : undefined
	);

	const chartConfig: ChartUI.ChartConfig = {
		critical: { label: "Critical", color: "hsl(0 72% 51%)" },
		warning: { label: "Warning", color: "hsl(25 95% 53%)" },
		optimal: { label: "Optimal", color: "hsl(142 71% 55%)" },
	};
</script>

<Dialog.Header class="px-6 pt-6 text-left">
	<Dialog.Title>VPD Zone Map</Dialog.Title>
	<Dialog.Description>
		{#if currentVpd !== undefined}
			Current: <span class="text-foreground font-medium">{currentVpd.toFixed(2)} kPa</span>
			· Target: {targetVpd.toFixed(1)} kPa
		{:else}
			Target: {targetVpd.toFixed(1)} kPa
		{/if}
	</Dialog.Description>
</Dialog.Header>

{#if !hasRequiredSensors}
	<div class="text-muted-foreground mx-6 flex h-64 w-auto items-center justify-center rounded-lg border border-dashed text-sm">
		Add temperature and humidity sensors to view VPD chart
	</div>
{:else}
	<ChartUI.ChartContainer config={chartConfig} class="aspect-auto h-64 w-full px-6 sm:h-72 [&_.lc-axis-tick-label]:text-[10px] sm:[&_.lc-axis-tick-label]:text-xs">
		<Chart
			xDomain={[HUM_MIN, HUM_MAX]}
			yDomain={[tempMin, tempMax]}
			xScale={scaleLinear()}
			yScale={scaleLinear()}
			padding={{ top: 8, bottom: 36, left: 44, right: 8 }}
		>
			{#snippet children({ context })}
				<Svg>
					<Axis
						placement="bottom"
						ticks={humTicks}
						format={(v) => `${v}%`}
					/>
					<Axis
						placement="left"
						ticks={tempTicks}
						format={(v) => `${Math.round(v as number)}°`}
					/>

					{#if true}
					{@const gridX = context.xScale(HUM_MIN)}
					{@const gridY = context.yScale(tempMax)}
					{@const gridW = context.xScale(HUM_MAX) - gridX}
					{@const gridH = context.yScale(tempMin) - gridY}
					<defs>
						<clipPath id="vpd-grid-clip">
							<rect x={gridX} y={gridY} width={gridW} height={gridH} rx="8" ry="8" />
						</clipPath>
					</defs>
					<g clip-path="url(#vpd-grid-clip)">
						{#each zoneRects as rect, i (i)}
							{@const x1 = context.xScale(rect.humMin)}
							{@const x2 = context.xScale(rect.humMax)}
							{@const y1 = context.yScale(convertTemperature(rect.tempMax, tempUnit))}
							{@const y2 = context.yScale(convertTemperature(rect.tempMin, tempUnit))}
							<rect
								x={x1}
								y={y1}
								width={x2 - x1 + 0.5}
								height={y2 - y1 + 0.5}
								fill={rect.color}
							/>
						{/each}
					</g>
				{/if}

					{#if currentTempDisplay !== undefined && currentHum !== undefined}
						{@const cx = context.xScale(currentHum)}
						{@const cy = context.yScale(currentTempDisplay)}
						<circle
							{cx}
							{cy}
							r="5"
							fill="rgba(255,255,255,0.3)"
							stroke="white"
							stroke-width="2"
						/>
					{/if}
				</Svg>
			{/snippet}
		</Chart>
	</ChartUI.ChartContainer>
{/if}

<div class="flex flex-wrap items-center gap-x-4 gap-y-1 px-6 pb-6">
	{#each Object.entries(chartConfig) as [key, cfg] (key)}
		<div class="flex items-center gap-1.5">
			<div class="size-2.5 rounded-[2px]" style="background-color: {cfg.color}"></div>
			<span class="text-xs text-muted-foreground">{cfg.label}</span>
		</div>
	{/each}
</div>
