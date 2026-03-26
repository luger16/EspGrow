<script lang="ts">
	import { Chart, Svg, Area, Axis } from "layerchart";
	import * as ChartUI from "$lib/components/ui/chart/index.js";
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import { scalePoint, scaleLinear } from "d3-scale";
	import { curveMonotoneX } from "d3-shape";
	import { spectralData } from "$lib/stores/sensors.svelte";

	const CHANNELS = [
		{ label: "415nm", color: "hsl(270 70% 60%)" },
		{ label: "445nm", color: "hsl(240 70% 60%)" },
		{ label: "480nm", color: "hsl(200 80% 55%)" },
		{ label: "515nm", color: "hsl(140 65% 45%)" },
		{ label: "555nm", color: "hsl(80 70% 45%)" },
		{ label: "590nm", color: "hsl(40 90% 50%)" },
		{ label: "630nm", color: "hsl(0 80% 50%)" },
		{ label: "680nm", color: "hsl(350 70% 38%)" },
	];

	const peakValue = $derived(
		spectralData.current ? Math.max(...spectralData.current.channels, 1) : 1
	);

	const chartData = $derived(
		spectralData.current
			? [
					{ label: "380nm", value: 0 },
					...CHANNELS.map((ch, i) => ({
						label: ch.label,
						value: Math.round(((spectralData.current!.channels[i] ?? 0) / peakValue) * 100),
					})),
					{ label: "730nm", value: 0 },
				]
			: []
	);

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
			yDomain={[0, 100]}
			padding={{ top: 8, bottom: 36, left: 50, right: 8 }}
		>
			<Svg>
				<defs>
					<linearGradient id="spectrum-h" x1="0" x2="1" y1="0" y2="0">
						{#each CHANNELS as ch, i (ch.label)}
							<stop
								offset="{(i / (CHANNELS.length - 1)) * 100}%"
								stop-color={ch.color}
							/>
						{/each}
					</linearGradient>
				</defs>
				<Axis placement="bottom" />
				<Axis placement="left" ticks={[0, 50, 100]} format={(v) => `${v}%`} />
				<Area
					fill="url(#spectrum-h)"
					line={{ stroke: "none" }}
					curve={curveMonotoneX}
				/>
			</Svg>
		</Chart>
	</ChartUI.ChartContainer>
{/if}
