<script lang="ts">
	import * as Card from "$lib/components/ui/card/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import Sun from "@lucide/svelte/icons/sun";
	import Moon from "@lucide/svelte/icons/moon";
	import Leaf from "@lucide/svelte/icons/leaf";
	import History from "@lucide/svelte/icons/history";
	import TriangleAlert from "@lucide/svelte/icons/triangle-alert";
	import {
		climateConfig,
		getIsDay,
		climateAlerts,
	} from "$lib/stores/climate.svelte";
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
	const activeAlerts = $derived(climateAlerts.length);
</script>

<Card.Root class="py-3">
	<Card.Content class="flex items-center justify-between px-4">
		<div class="flex items-center gap-3">
			<div class="flex items-center gap-1.5">
				<Leaf class="size-4 text-muted-foreground" />
				<span class="text-sm font-medium">{activePhaseLabel}</span>
			</div>
			<span class="text-muted-foreground/40">·</span>
			<div class="flex items-center gap-1 text-sm text-muted-foreground">
				<DayNightIcon class="size-3.5" />
				<span>{dayNightText}</span>
			</div>
			{#if activeAlerts > 0}
				<span class="text-muted-foreground/40">·</span>
				<div class="flex items-center gap-1 text-sm text-amber-500">
					<TriangleAlert class="size-3.5" />
					<span>{activeAlerts}</span>
				</div>
			{/if}
		</div>
		<Button variant="outline" size="icon" class="relative size-8" onclick={onalertclick}>
			<History class="size-4" />
			{#if activeAlerts > 0}
				<span class="absolute -right-1.5 -top-1.5 flex size-4 items-center justify-center rounded-full bg-amber-500/70 text-[10px] font-medium text-white tabular-nums">
					{activeAlerts}
				</span>
			{/if}
		</Button>
	</Card.Content>
</Card.Root>
