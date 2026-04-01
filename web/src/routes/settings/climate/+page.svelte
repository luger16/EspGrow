<script lang="ts">
	import PageHeader from "$lib/components/page-header.svelte";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import * as Tabs from "$lib/components/ui/tabs/index.js";

	import { sensors } from "$lib/stores/sensors.svelte";
	import { climateConfig, setActivePhase, setDayNightMode, setManualSchedule, setLightThreshold, updatePhaseTargets, resetPhaseTargets } from "$lib/stores/climate.svelte";
	import { DEFAULT_PHASE_TARGETS } from "$lib/climate-presets";
	import type { ClimatePhase } from "$lib/types";
	import ArrowLeftIcon from "@lucide/svelte/icons/arrow-left";
	import SunIcon from "@lucide/svelte/icons/sun";
	import MoonIcon from "@lucide/svelte/icons/moon";
	import RotateCcwIcon from "@lucide/svelte/icons/rotate-ccw";

	const phaseOptions: { value: ClimatePhase; label: string }[] = [
		{ value: "seedling", label: "Seedling" },
		{ value: "veg", label: "Vegetative" },
		{ value: "flower", label: "Flower" },
		{ value: "dry", label: "Drying" },
	];

	const dayNightOptions: { value: "auto" | "manual"; label: string }[] = [
		{ value: "auto", label: "Auto (light sensor)" },
		{ value: "manual", label: "Manual schedule" },
	];

	const hasAs7341 = $derived(sensors.some((s) => s.hardwareType === "as7341"));

	const activeTargets = $derived(climateConfig.phases[climateConfig.activePhase]);
	const defaultTargets = $derived(DEFAULT_PHASE_TARGETS[climateConfig.activePhase]);
	const targetsModified = $derived(JSON.stringify(activeTargets) !== JSON.stringify(defaultTargets));

	function updateTarget(key: "temp" | "humidity" | "vpd" | "co2", period: "day" | "night", value: number): void {
		const phase = climateConfig.activePhase;
		const targets = { ...climateConfig.phases[phase] };
		targets[key] = { ...targets[key], [period]: value };
		updatePhaseTargets(phase, targets);
	}

	function updateDliTarget(value: number): void {
		const phase = climateConfig.activePhase;
		const targets = { ...climateConfig.phases[phase], dli: value };
		updatePhaseTargets(phase, targets);
	}
</script>

<PageHeader title="Climate" />
<div class="flex flex-1 flex-col gap-6 p-4 pt-0">
	<div class="flex items-center gap-2">
		<Button variant="ghost" size="icon" href="/settings">
			<ArrowLeftIcon class="size-4" />
		</Button>
		<h2 class="text-sm font-medium text-muted-foreground">Climate Settings</h2>
	</div>

	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Growth Phase</h2>
		<div class="divide-y divide-border rounded-lg border">
			<div class="flex items-center justify-between p-3">
				<Label>Phase</Label>
				<div class="flex items-center gap-1.5">
					{#if targetsModified}
						<Button variant="ghost" size="sm" class="h-7 gap-1 px-2 text-xs text-muted-foreground" onclick={() => resetPhaseTargets(climateConfig.activePhase)}>
							<RotateCcwIcon class="size-3" />
							Reset
						</Button>
					{/if}
					<Tabs.Root
						value={climateConfig.activePhase}
						onValueChange={(v) => v && setActivePhase(v as ClimatePhase)}
					>
						<Tabs.List class="h-7">
							{#each phaseOptions as option (option.value)}
								<Tabs.Trigger value={option.value} class="px-2 text-xs">{option.label}</Tabs.Trigger>
							{/each}
						</Tabs.List>
					</Tabs.Root>
				</div>
			</div>
			<div class="flex items-center justify-between p-3">
				<Label class="text-muted-foreground">Temp (°C)</Label>
				<div class="flex items-center gap-2">
					<SunIcon class="size-3 text-muted-foreground" />
					<Input type="number" value={activeTargets.temp.day} onchange={(e) => updateTarget("temp", "day", Number((e.target as HTMLInputElement).value))} class="h-7 w-20 text-xs" />
					<MoonIcon class="size-3 text-muted-foreground" />
					<Input type="number" value={activeTargets.temp.night} onchange={(e) => updateTarget("temp", "night", Number((e.target as HTMLInputElement).value))} class="h-7 w-20 text-xs" />
				</div>
			</div>
			<div class="flex items-center justify-between p-3">
				<Label class="text-muted-foreground">Humidity (%)</Label>
				<div class="flex items-center gap-2">
					<SunIcon class="size-3 text-muted-foreground" />
					<Input type="number" value={activeTargets.humidity.day} onchange={(e) => updateTarget("humidity", "day", Number((e.target as HTMLInputElement).value))} class="h-7 w-20 text-xs" />
					<MoonIcon class="size-3 text-muted-foreground" />
					<Input type="number" value={activeTargets.humidity.night} onchange={(e) => updateTarget("humidity", "night", Number((e.target as HTMLInputElement).value))} class="h-7 w-20 text-xs" />
				</div>
			</div>
			<div class="flex items-center justify-between p-3">
				<Label class="text-muted-foreground">VPD (kPa)</Label>
				<div class="flex items-center gap-2">
					<SunIcon class="size-3 text-muted-foreground" />
					<Input type="number" step="0.1" value={activeTargets.vpd.day} onchange={(e) => updateTarget("vpd", "day", Number((e.target as HTMLInputElement).value))} class="h-7 w-20 text-xs" />
					<MoonIcon class="size-3 text-muted-foreground" />
					<Input type="number" step="0.1" value={activeTargets.vpd.night} onchange={(e) => updateTarget("vpd", "night", Number((e.target as HTMLInputElement).value))} class="h-7 w-20 text-xs" />
				</div>
			</div>
			<div class="flex items-center justify-between p-3">
				<Label class="text-muted-foreground">CO₂ (ppm)</Label>
				<div class="flex items-center gap-2">
					<SunIcon class="size-3 text-muted-foreground" />
					<Input type="number" step="50" value={activeTargets.co2.day} onchange={(e) => updateTarget("co2", "day", Number((e.target as HTMLInputElement).value))} class="h-7 w-20 text-xs" />
					<MoonIcon class="size-3 text-muted-foreground" />
					<Input type="number" step="50" value={activeTargets.co2.night} onchange={(e) => updateTarget("co2", "night", Number((e.target as HTMLInputElement).value))} class="h-7 w-20 text-xs" />
				</div>
			</div>
			{#if hasAs7341}
				<div class="flex items-center justify-between p-3">
					<Label class="text-muted-foreground">DLI (mol/m²/d)</Label>
					<Input type="number" value={activeTargets.dli} onchange={(e) => updateDliTarget(Number((e.target as HTMLInputElement).value))} class="h-7 w-20 text-xs" />
				</div>
			{/if}
		</div>
	</section>

	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Day/Night Detection</h2>
		<div class="divide-y divide-border rounded-lg border">
			<div class="flex items-center justify-between p-3">
				<Label>Mode</Label>
				<Tabs.Root
					value={climateConfig.dayNightMode}
					onValueChange={(v) => v && setDayNightMode(v as "auto" | "manual")}
				>
					<Tabs.List class="h-7">
						{#each dayNightOptions as option (option.value)}
							<Tabs.Trigger value={option.value} class="px-2 text-xs">{option.label}</Tabs.Trigger>
						{/each}
					</Tabs.List>
				</Tabs.Root>
			</div>
			{#if climateConfig.dayNightMode === "manual"}
				<div class="flex items-center justify-between p-3">
					<Label class="text-muted-foreground">Schedule</Label>
					<div class="flex items-center gap-3">
						<div class="flex items-center gap-1.5">
							<SunIcon class="size-3.5 text-muted-foreground" />
							<Input
								type="time"
								value={climateConfig.manualSchedule?.dayStart || "06:00"}
								onchange={(e) => {
									const target = e.target as HTMLInputElement;
									const dayStart = target.value;
									const nightStart = climateConfig.manualSchedule?.nightStart || "18:00";
									setManualSchedule(dayStart, nightStart);
								}}
								class="h-7 w-24 text-xs"
							/>
						</div>
						<div class="flex items-center gap-1.5">
							<MoonIcon class="size-3.5 text-muted-foreground" />
							<Input
								type="time"
								value={climateConfig.manualSchedule?.nightStart || "18:00"}
								onchange={(e) => {
									const target = e.target as HTMLInputElement;
									const nightStart = target.value;
									const dayStart = climateConfig.manualSchedule?.dayStart || "06:00";
									setManualSchedule(dayStart, nightStart);
								}}
								class="h-7 w-24 text-xs"
							/>
						</div>
					</div>
				</div>
			{:else}
				<div class="flex items-center justify-between p-3">
					<Label class="text-muted-foreground">Threshold</Label>
					<div class="flex items-center gap-2">
						<Input
							type="number"
							value={climateConfig.lightThreshold}
							onchange={(e) => {
								const target = e.target as HTMLInputElement;
								setLightThreshold(Number(target.value));
							}}
							class="h-7 w-20 text-xs"
						/>
						<span class="text-xs text-muted-foreground">PPFD</span>
					</div>
				</div>
			{/if}
		</div>
	</section>
</div>
