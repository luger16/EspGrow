<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import { climateAlerts, formatAlertTitle, formatAlertDescription } from "$lib/stores/climate.svelte";
	import { sensors } from "$lib/stores/sensors.svelte";
	import { formatTimeFromDate } from "$lib/stores/settings.svelte";
	import { cn } from "$lib/utils";

	let {
		open = $bindable(false),
		onOpenChange,
	}: {
		open?: boolean;
		onOpenChange?: (open: boolean) => void;
	} = $props();

	const recentAlerts = $derived(climateAlerts.slice(0, 50));

	function getSensorName(sensorId: string): string {
		const sensor = sensors.find((s) => s.id === sensorId);
		return sensor?.name || "Unknown Sensor";
	}
</script>

<Dialog.Root {open} {onOpenChange}>
	<Dialog.Content class="w-full max-w-[calc(100%-1rem)] gap-0 overflow-hidden p-0 sm:max-w-lg" showCloseButton={false}>
		<div class="flex items-center justify-between gap-2 px-4 pt-4 pb-3 sm:px-6 sm:pt-6">
			<Dialog.Title class="text-lg font-semibold leading-tight">Alert History</Dialog.Title>
			<Dialog.Close
				class="ring-offset-background focus:ring-ring rounded-sm opacity-70 transition-opacity hover:opacity-100 focus:ring-2 focus:ring-offset-2 focus:outline-hidden disabled:pointer-events-none"
			>
				<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M18 6 6 18"/><path d="m6 6 12 12"/></svg>
				<span class="sr-only">Close</span>
			</Dialog.Close>
		</div>

		{#if recentAlerts.length === 0}
			<div class="text-muted-foreground flex h-64 items-center justify-center px-4 pb-4 text-sm sm:px-6 sm:pb-6">
				No alerts yet
			</div>
		{:else}
			<div class="max-h-96 overflow-y-auto px-4 pb-4 sm:px-6 sm:pb-6">
				<div class="divide-y divide-border rounded-md border">
					{#each recentAlerts as alert (alert.id)}
						<div class="flex items-start gap-3 px-3 py-2.5">
							<div class={cn(
								"mt-1.5 size-2 rounded-full shrink-0",
								alert.severity === "critical" ? "bg-destructive" : "bg-yellow-500"
							)}></div>

							<div class="flex-1 min-w-0">
								<div class="flex items-center gap-2">
									<span class="text-sm font-medium truncate">
										{formatAlertTitle(alert)}
									</span>
									<span class={cn(
										"shrink-0 rounded px-1 py-0.5 text-[10px] font-medium leading-none uppercase",
										alert.severity === "critical"
											? "bg-destructive/10 text-destructive"
											: "bg-yellow-500/10 text-yellow-600 dark:text-yellow-500"
									)}>
										{alert.severity}
									</span>
									<span class="ml-auto shrink-0 text-xs tabular-nums text-muted-foreground">
										{formatTimeFromDate(alert.timestamp)}
									</span>
								</div>
								<p class="mt-0.5 text-xs text-muted-foreground">
									{formatAlertDescription(alert)}
								</p>
								<span class="mt-0.5 text-[11px] text-muted-foreground/70">
									{getSensorName(alert.sensorId)}
								</span>
							</div>
						</div>
					{/each}
				</div>
			</div>
		{/if}
	</Dialog.Content>
</Dialog.Root>
