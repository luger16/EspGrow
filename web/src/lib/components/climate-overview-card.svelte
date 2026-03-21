<script lang="ts">
	import * as Card from "$lib/components/ui/card/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import Sun from "@lucide/svelte/icons/sun";
	import Moon from "@lucide/svelte/icons/moon";
	import Leaf from "@lucide/svelte/icons/leaf";
	import Activity from "@lucide/svelte/icons/activity";
	import Plug from "@lucide/svelte/icons/plug";
	import TriangleAlert from "@lucide/svelte/icons/triangle-alert";
	import {
		climateConfig,
		getIsDay,
		getHealthScore,
		climateAlerts,
	} from "$lib/stores/climate.svelte";
	import { sensors } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import { cn } from "$lib/utils";

	let { onalertclick }: { onalertclick?: () => void } = $props();

	const PHASE_LABELS: Record<string, string> = {
		seedling: "Seedling",
		veg: "Vegetative",
		flower: "Flower",
		dry: "Drying",
	};

	const activePhaseLabel = $derived(PHASE_LABELS[climateConfig.activePhase] ?? climateConfig.activePhase);
	const dayNight = $derived(getIsDay());
	const DayNightIcon = $derived(dayNight ? Sun : Moon);
	const dayNightText = $derived(dayNight ? "Day" : "Night");
	const healthScore = $derived(getHealthScore());
	const hasSensors = $derived(sensors.length > 0);

	const healthColor = $derived.by(() => {
		if (healthScore >= 80) return "text-green-500";
		if (healthScore >= 50) return "text-amber-500";
		return "text-red-500";
	});

	const devicesOn = $derived(devices.filter((d) => d.isOn).length);
	const activeAlerts = $derived(climateAlerts.length);
</script>

<Card.Root class="py-3">
	<Card.Content class="px-4">
		<div class="flex items-center gap-4 flex-wrap">
			<div class="flex items-center gap-2">
				<Leaf class="size-4 text-muted-foreground" />
				<span class="text-sm font-medium">{activePhaseLabel}</span>
				<div class="flex items-center gap-1 text-xs text-muted-foreground">
					<DayNightIcon class="size-3" />
					<span>{dayNightText}</span>
				</div>
			</div>

			<div class="h-4 w-px bg-border hidden sm:block"></div>

			{#if hasSensors}
				<div class="flex items-center gap-1.5 text-xs text-muted-foreground">
					<Activity class="size-3" />
					<span>{sensors.length} sensor{sensors.length !== 1 ? "s" : ""}</span>
				</div>

				<div class="flex items-center gap-1.5 text-xs text-muted-foreground">
					<span class={cn("text-sm font-semibold tabular-nums", healthColor)}>
						{Math.round(healthScore)}%
					</span>
					<span>health</span>
				</div>
			{/if}

			{#if devices.length > 0}
				<div class="flex items-center gap-1.5 text-xs text-muted-foreground">
					<Plug class="size-3" />
					<span>{devicesOn}/{devices.length} on</span>
				</div>
			{/if}

			<div class="ml-auto">
				{#if activeAlerts > 0}
					<Button
						variant="outline"
						size="sm"
						onclick={onalertclick}
						class="text-amber-500 hover:text-amber-400"
					>
						<TriangleAlert class="size-3" />
						{activeAlerts} alert{activeAlerts !== 1 ? "s" : ""}
					</Button>
				{:else}
					<Button
						variant="outline"
						size="sm"
						onclick={onalertclick}
						class="text-muted-foreground"
					>
						<TriangleAlert class="size-3" />
						0 alerts
					</Button>
				{/if}
			</div>
		</div>
	</Card.Content>
</Card.Root>
