<script lang="ts">
	import * as Card from "$lib/components/ui/card/index.js";
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import Sun from "@lucide/svelte/icons/sun";
	import Moon from "@lucide/svelte/icons/moon";
	import Leaf from "@lucide/svelte/icons/leaf";
	import History from "@lucide/svelte/icons/history";
	import Zap from "@lucide/svelte/icons/zap";
	import RotateCcw from "@lucide/svelte/icons/rotate-ccw";
	import TriangleAlert from "@lucide/svelte/icons/triangle-alert";
	import {
		climateConfig,
		getIsDay,
		climateAlerts,
		systemEvents,
	} from "$lib/stores/climate.svelte";
	import {
		deviceEnergies,
		getTotalWatts,
		getTotalKWh,
		resetEnergy,
	} from "$lib/stores/energy.svelte";

	let { onhistoryclick }: { onhistoryclick?: () => void } = $props();

	let energyDialogOpen = $state(false);
	let confirmResetId = $state<string | null>(null);

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
	const eventCount = $derived(systemEvents.length);
	const totalWatts = $derived(getTotalWatts());
	const totalKWh = $derived(getTotalKWh());
	const hasEnergy = $derived(deviceEnergies.length > 0);

	function formatSince(date: Date): string {
		const days = Math.floor((Date.now() - date.getTime()) / (1000 * 60 * 60 * 24));
		if (days === 0) return "today";
		if (days === 1) return "1 day ago";
		return `${days} days ago`;
	}

	function handleReset(deviceId?: string): void {
		resetEnergy(deviceId);
		confirmResetId = null;
	}
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
			{#if hasEnergy}
				<span class="text-muted-foreground/40">·</span>
				<button
					class="flex items-center gap-1 text-sm text-muted-foreground transition-colors hover:text-foreground"
					onclick={() => (energyDialogOpen = true)}
				>
					<Zap class="size-3.5" />
					<span class="tabular-nums">{Math.round(totalWatts)}W</span>
					<span class="text-muted-foreground/60">·</span>
					<span class="tabular-nums">{totalKWh} kWh</span>
				</button>
			{/if}
		</div>
		<Button variant="outline" size="icon" class="relative size-8" onclick={onhistoryclick}>
			<History class="size-4" />
			{#if eventCount > 0}
				<span class="absolute -right-1.5 -top-1.5 flex size-4 items-center justify-center rounded-full bg-primary/70 text-[10px] font-medium text-primary-foreground tabular-nums">
					{eventCount > 99 ? "99" : eventCount}
				</span>
			{/if}
		</Button>
	</Card.Content>
</Card.Root>

<Dialog.Root bind:open={energyDialogOpen}>
	<Dialog.Content class="max-w-sm">
		<Dialog.Header>
			<Dialog.Title class="flex items-center gap-2">
				<Zap class="size-4" />
				Energy
			</Dialog.Title>
			<Dialog.Description>
				Total: {Math.round(totalWatts)}W · {totalKWh} kWh
			</Dialog.Description>
		</Dialog.Header>
		<div class="flex flex-col gap-2">
			{#each deviceEnergies as entry (entry.deviceId)}
				<div class="flex items-center justify-between rounded-md border px-3 py-2">
					<div class="flex flex-col gap-0.5">
						<span class="text-sm font-medium">{entry.deviceName}</span>
						<span class="text-xs text-muted-foreground tabular-nums">
							{entry.watts.toFixed(1)}W · {entry.kWh.toFixed(2)} kWh
						</span>
						<span class="text-xs text-muted-foreground">
							Since {formatSince(entry.resetTimestamp)}
						</span>
					</div>
					{#if confirmResetId === entry.deviceId}
						<div class="flex items-center gap-1">
							<Button variant="destructive" size="sm" onclick={() => handleReset(entry.deviceId)}>
								Reset
							</Button>
							<Button variant="ghost" size="sm" onclick={() => (confirmResetId = null)}>
								Cancel
							</Button>
						</div>
					{:else}
						<Button
							variant="ghost"
							size="icon"
							class="size-7"
							onclick={() => (confirmResetId = entry.deviceId)}
						>
							<RotateCcw class="size-3.5" />
						</Button>
					{/if}
				</div>
			{/each}
		</div>
	</Dialog.Content>
</Dialog.Root>
