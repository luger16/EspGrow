<script lang="ts">
	import * as Card from "$lib/components/ui/card/index.js";
	import { Switch } from "$lib/components/ui/switch/index.js";
	import { Badge } from "$lib/components/ui/badge/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import type { Device, DeviceMode } from "$lib/types";
	import { toggleDevice, pendingDevices, overriddenDevices, clearOverride } from "$lib/stores/devices.svelte";
	import { getDeviceMode } from "$lib/stores/device-modes.svelte";
	import { deviceIcons } from "$lib/icons";
	import LoaderIcon from "@lucide/svelte/icons/loader-circle";
	import TriangleAlertIcon from "@lucide/svelte/icons/triangle-alert";
	import WifiOffIcon from "@lucide/svelte/icons/wifi-off";

	let { device }: { device: Device } = $props();

	const Icon = $derived(deviceIcons[device.type]);
	const isPending = $derived(pendingDevices.has(device.id));
	const isOnline = $derived(device.isOnline !== false);
	const modeConfig = $derived(getDeviceMode(device.id));
	const isOverridden = $derived(
		device.controlMode === "automatic" &&
		overriddenDevices[device.id] !== undefined &&
		overriddenDevices[device.id] > Date.now()
	);

	const modeBadgeLabel: Record<DeviceMode, string> = {
		off: "Off",
		on: "On",
		auto: "Auto",
		cycle: "Cycle",
		schedule: "Sched",
	};

	let now = $state(Date.now());

	$effect(() => {
		if (!isOverridden) return;
		const interval = setInterval(() => {
			now = Date.now();
		}, 1000);
		return () => clearInterval(interval);
	});

	const timeRemaining = $derived.by(() => {
		if (!isOverridden) return "";
		const remaining = overriddenDevices[device.id] - now;
		if (remaining <= 0) return "";
		const minutes = Math.floor(remaining / 60000);
		const seconds = Math.floor((remaining % 60000) / 1000);
		return `${minutes}:${seconds.toString().padStart(2, '0')}`;
	});

	function handleToggle() {
		toggleDevice(device.id);
	}

	function handleClearOverride() {
		clearOverride(device.id);
	}
</script>

<Card.Root class="flex flex-col py-2.5 {!isOnline ? 'opacity-60' : ''}">
	<Card.Content class="flex items-center gap-3 px-3">
		<div
			class="flex size-7 shrink-0 items-center justify-center rounded-md {device.isOn && isOnline
				? 'bg-primary/10 text-primary'
				: 'bg-muted text-muted-foreground'}"
		>
			<Icon class="size-3.5" />
		</div>
		<div class="min-w-0 flex-1">
			<p class="truncate text-xs text-muted-foreground">{device.name}</p>
			<div class="flex items-center gap-1.5">
				{#if !isOnline}
					<span class="text-base font-semibold text-muted-foreground">Offline</span>
					<Badge variant="destructive" class="text-[10px] px-1 py-0 gap-0.5">
						<WifiOffIcon class="size-2.5" />
						Offline
					</Badge>
				{:else}
					<span class="text-base font-semibold">{device.isOn ? "On" : "Off"}</span>
					{#if isOverridden}
						<Badge variant="destructive" class="text-[10px] px-1 py-0 gap-0.5">
							<TriangleAlertIcon class="size-2.5" />
							Override
						</Badge>
					{:else if modeConfig}
						<Badge variant="secondary" class="text-[10px] px-1 py-0">
							{modeBadgeLabel[modeConfig.mode]}
						</Badge>
					{:else}
						<Badge variant="outline" class="text-[10px] px-1 py-0">Manual</Badge>
					{/if}
				{/if}
			</div>
		</div>
		{#if !isOnline}
			<WifiOffIcon class="size-4 text-destructive" />
		{:else if isPending}
			<LoaderIcon class="size-4 animate-spin text-muted-foreground" />
		{:else}
			<Switch checked={device.isOn} onCheckedChange={handleToggle} />
		{/if}
	</Card.Content>
	{#if isOverridden}
		<Card.Content class="flex items-center justify-between gap-2 px-3 pt-2 border-t mt-2">
			<span class="text-muted-foreground text-[10px]">Override ({timeRemaining})</span>
			<Button variant="outline" size="sm" class="h-6 text-[10px] px-2" onclick={handleClearOverride}>
				Resume
			</Button>
		</Card.Content>
	{/if}
</Card.Root>
