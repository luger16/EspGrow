<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import { systemEvents } from "$lib/stores/climate.svelte";
	import { formatTimeFromDate } from "$lib/stores/settings.svelte";
	import { cn } from "$lib/utils";
	import TriangleAlert from "@lucide/svelte/icons/triangle-alert";
	import Zap from "@lucide/svelte/icons/zap";
	import Power from "@lucide/svelte/icons/power";
	import Info from "@lucide/svelte/icons/info";
	import type { SystemEventType } from "$lib/types";

	let {
		open = $bindable(false),
		onOpenChange,
	}: {
		open?: boolean;
		onOpenChange?: (open: boolean) => void;
	} = $props();

	const recentEvents = $derived(systemEvents.slice(0, 50));

	const EVENT_ICONS: Record<SystemEventType, typeof TriangleAlert> = {
		alert: TriangleAlert,
		automation: Zap,
		device: Power,
		system: Info,
	};

	const EVENT_COLORS: Record<SystemEventType, string> = {
		alert: "text-amber-500",
		automation: "text-blue-500",
		device: "text-muted-foreground",
		system: "text-muted-foreground",
	};

	const EVENT_LABELS: Record<SystemEventType, string> = {
		alert: "Alert",
		automation: "Automation",
		device: "Device",
		system: "System",
	};
</script>

<Dialog.Root {open} {onOpenChange}>
	<Dialog.Content class="w-full max-w-[calc(100%-1rem)] gap-0 overflow-hidden p-0 sm:max-w-lg" showCloseButton={false}>
		<div class="flex items-center justify-between gap-2 px-4 pt-4 pb-3 sm:px-6 sm:pt-6">
			<Dialog.Title class="text-lg font-semibold leading-tight">Event History</Dialog.Title>
			<Dialog.Close
				class="ring-offset-background focus:ring-ring rounded-sm opacity-70 transition-opacity hover:opacity-100 focus:ring-2 focus:ring-offset-2 focus:outline-hidden disabled:pointer-events-none"
			>
				<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M18 6 6 18"/><path d="m6 6 12 12"/></svg>
				<span class="sr-only">Close</span>
			</Dialog.Close>
		</div>

		{#if recentEvents.length === 0}
			<div class="text-muted-foreground flex h-64 items-center justify-center px-4 pb-4 text-sm sm:px-6 sm:pb-6">
				No events yet
			</div>
		{:else}
			<div class="max-h-96 overflow-y-auto px-4 pb-4 sm:px-6 sm:pb-6">
				<div class="divide-y divide-border rounded-md border">
					{#each recentEvents as event (event.id)}
						{@const Icon = EVENT_ICONS[event.type]}
						<div class="flex items-start gap-3 px-3 py-2.5">
							<Icon class={cn("mt-0.5 size-4 shrink-0", event.type === "alert" && event.severity === "critical" ? "text-destructive" : EVENT_COLORS[event.type])} />
							<div class="flex-1 min-w-0">
								<div class="flex items-center gap-2">
									<span class="text-sm font-medium truncate">{event.title}</span>
									<span class={cn(
										"shrink-0 rounded px-1 py-0.5 text-[10px] font-medium leading-none uppercase",
										event.type === "alert" && event.severity === "critical"
											? "bg-destructive/10 text-destructive"
											: event.type === "alert"
												? "bg-yellow-500/10 text-yellow-600 dark:text-yellow-500"
												: "bg-muted text-muted-foreground"
									)}>
										{event.type === "alert" ? event.severity : EVENT_LABELS[event.type]}
									</span>
									<span class="ml-auto shrink-0 text-xs tabular-nums text-muted-foreground">
										{formatTimeFromDate(event.timestamp)}
									</span>
								</div>
								<p class="mt-0.5 text-xs text-muted-foreground">
									{event.description}
								</p>
							</div>
						</div>
					{/each}
				</div>
			</div>
		{/if}
	</Dialog.Content>
</Dialog.Root>
