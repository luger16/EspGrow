<script lang="ts">
	import * as Card from "$lib/components/ui/card/index.js";
	import { Switch } from "$lib/components/ui/switch/index.js";
	import { Badge } from "$lib/components/ui/badge/index.js";
	import type { Device, DeviceMode } from "$lib/types";
	import { toggleDevice, pendingDevices } from "$lib/stores/devices.svelte";
	import { getDeviceMode } from "$lib/stores/device-modes.svelte";
	import { deviceIcons } from "$lib/icons";
	import LoaderIcon from "@lucide/svelte/icons/loader-circle";
	import WifiOffIcon from "@lucide/svelte/icons/wifi-off";

	let { device }: { device: Device } = $props();

	const Icon = $derived(deviceIcons[device.type]);
	const isPending = $derived(pendingDevices.has(device.id));
	const isOnline = $derived(device.isOnline !== false);
	const modeConfig = $derived(getDeviceMode(device.id));

	const modeBadgeLabel: Record<DeviceMode, string> = {
		off: "Off",
		on: "On",
		auto: "Auto",
		cycle: "Cycle",
		schedule: "Sched",
	};


	function handleToggle() {
		toggleDevice(device.id);
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
					{#if modeConfig}
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
</Card.Root>
