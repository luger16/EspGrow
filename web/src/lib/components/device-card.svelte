<script lang="ts">
	import * as Card from "$lib/components/ui/card/index.js";
	import { Switch } from "$lib/components/ui/switch/index.js";
	import { Badge } from "$lib/components/ui/badge/index.js";
	import type { Device } from "$lib/types";
	import { toggleDevice, pendingDevices, overriddenDevices } from "$lib/stores/devices.svelte";
	import { deviceIcons } from "$lib/icons";
	import LoaderIcon from "@lucide/svelte/icons/loader-circle";
	import TriangleAlertIcon from "@lucide/svelte/icons/triangle-alert";

	let { device }: { device: Device } = $props();

	const Icon = $derived(deviceIcons[device.type]);
	const isPending = $derived(pendingDevices.has(device.id));
	const isOverridden = $derived(
		device.controlMode === "automatic" &&
		overriddenDevices[device.id] !== undefined &&
		overriddenDevices[device.id] > Date.now()
	);

	function handleToggle() {
		toggleDevice(device.id);
	}
</script>

<Card.Root class="flex flex-col py-4">
	<Card.Content class="flex items-center gap-4 px-4">
		<div
			class="flex size-10 shrink-0 items-center justify-center rounded-lg {device.isOn
				? 'bg-primary/10 text-primary'
				: 'bg-muted text-muted-foreground'}"
		>
			<Icon class="size-5" />
		</div>
		<div class="flex flex-1 flex-col gap-1">
			<span class="font-medium">{device.name}</span>
			<div class="flex items-center gap-2">
				<span class="text-muted-foreground text-xs">{device.isOn ? "On" : "Off"}</span>
				{#if isOverridden}
					<Badge variant="destructive" class="text-xs px-1.5 py-0 gap-1">
						<TriangleAlertIcon class="size-3" />
						Override
					</Badge>
				{:else}
					<Badge variant={device.controlMode === "automatic" ? "secondary" : "outline"} class="text-xs px-1.5 py-0">
						{device.controlMode === "automatic" ? "Auto" : "Manual"}
					</Badge>
				{/if}
			</div>
		</div>
		{#if isPending}
			<LoaderIcon class="size-5 animate-spin text-muted-foreground" />
		{:else}
			<Switch checked={device.isOn} onCheckedChange={handleToggle} />
		{/if}
	</Card.Content>
</Card.Root>
