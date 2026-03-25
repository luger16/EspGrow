<script lang="ts">
	import { Chart, Svg, Area, Axis } from "layerchart";
	import * as ChartUI from "$lib/components/ui/chart/index.js";
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import { scalePoint, scaleLinear } from "d3-scale";
	import { curveMonotoneX } from "d3-shape";
	import { spectralData } from "$lib/stores/sensors.svelte";

	const CHANNELS = [
		{ label: "415nm", color: "hsl(270 70% 60%)", colorAlpha: "hsl(270 70% 60% / 0.4)" },
		{ label: "445nm", color: "hsl(240 70% 60%)", colorAlpha: "hsl(240 70% 60% / 0.4)" },
		{ label: "480nm", color: "hsl(200 80% 55%)", colorAlpha: "hsl(200 80% 55% / 0.4)" },
		{ label: "515nm", color: "hsl(140 65% 45%)", colorAlpha: "hsl(140 65% 45% / 0.4)" },
		{ label: "555nm", color: "hsl(80 70% 45%)", colorAlpha: "hsl(80 70% 45% / 0.4)" },
		{ label: "590nm", color: "hsl(40 90% 50%)", colorAlpha: "hsl(40 90% 50% / 0.4)" },
		{ label: "630nm", color: "hsl(10 80% 50%)", colorAlpha: "hsl(10 80% 50% / 0.4)" },
		{ label: "680nm", color: "hsl(0 70% 45%)", colorAlpha: "hsl(0 70% 45% / 0.4)" },
	];

	const chartData = $derived(
		spectralData.current
			? CHANNELS.map((ch, i) => ({
					label: ch.label,
					value: spectralData.current!.channels[i] ?? 0,
				}))
			: []
	);

	const maxValue = $derived(Math.max(...chartData.map((d) => d.value), 1));

	const chartConfig: ChartUI.ChartConfig = {
		spectrum: { label: "Intensity", color: "hsl(0 0% 50%)" },
	};
</script>

<Dialog.Header class="px-6 pt-6 text-left">
	<Dialog.Title>Light Spectrum</Dialog.Title>
	<Dialog.Description>AS7341 spectral channels (F1–F8)</Dialog.Description>
</Dialog.Header>

{#if !spectralData.current}
	<div
		class="text-muted-foreground mx-6 flex h-64 w-auto items-center justify-center rounded-lg border border-dashed text-sm"
	>
		No spectral data available
	</div>
{:else}
	<ChartUI.ChartContainer
		config={chartConfig}
		class="aspect-auto h-64 w-full px-6 pb-6 sm:h-72 [&_.lc-axis-tick-label]:text-[10px] sm:[&_.lc-axis-tick-label]:text-xs"
	>
		<Chart
			data={chartData}
			x="label"
			xScale={scalePoint().padding(0.1)}
			y="value"
			yScale={scaleLinear()}
			yDomain={[0, maxValue]}
			padding={{ top: 8, bottom: 36, left: 50, right: 8 }}
		>
			<Svg>
				<defs>
					<linearGradient id="spectrum-fill" x1="0" x2="1" y1="0" y2="0">
						{#each CHANNELS as ch, i (ch.label)}
							<stop
								offset="{(i / (CHANNELS.length - 1)) * 100}%"
								stop-color={ch.colorAlpha}
							/>
						{/each}
					</linearGradient>
					<linearGradient id="spectrum-stroke" x1="0" x2="1" y1="0" y2="0">
						{#each CHANNELS as ch, i (ch.label)}
							<stop
								offset="{(i / (CHANNELS.length - 1)) * 100}%"
								stop-color={ch.color}
							/>
						{/each}
					</linearGradient>
				</defs>
				<Axis placement="bottom" />
				<Axis placement="left" format={(v) => `${v}`} />
				<Area
					fill="url(#spectrum-fill)"
					line={{ stroke: "url(#spectrum-stroke)", strokeWidth: 2 }}
					curve={curveMonotoneX}
				/>
			</Svg>
		</Chart>
	</ChartUI.ChartContainer>
{/if}
