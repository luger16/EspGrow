<script lang="ts">
	import * as Card from "$lib/components/ui/card/index.js";
	import { Switch } from "$lib/components/ui/switch/index.js";
	import { Badge } from "$lib/components/ui/badge/index.js";
	import type { Device } from "$lib/types";
	import { toggleDevice } from "$lib/stores/devices.svelte";
	import FanIcon from "@lucide/svelte/icons/fan";
	import LightbulbIcon from "@lucide/svelte/icons/lightbulb";
	import FlameIcon from "@lucide/svelte/icons/flame";
	import DropletIcon from "@lucide/svelte/icons/droplet";
	import SparklesIcon from "@lucide/svelte/icons/sparkles";
	import type { Component } from "svelte";

	let { device }: { device: Device } = $props();

	const iconMap: Record<Device["type"], Component> = {
		fan: FanIcon,
		light: LightbulbIcon,
		heater: FlameIcon,
		pump: DropletIcon,
		humidifier: SparklesIcon,
	};

	const Icon = $derived(iconMap[device.type]);

	function handleToggle() {
		toggleDevice(device.id);
	}
</script>

<Card.Root class="flex flex-col">
	<Card.Content class="flex items-center gap-4 p-4">
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
				<Badge variant={device.controlMode === "automatic" ? "secondary" : "outline"} class="text-xs px-1.5 py-0">
					{device.controlMode === "automatic" ? "Auto" : "Manual"}
				</Badge>
			</div>
		</div>
		<Switch checked={device.isOn} onCheckedChange={handleToggle} />
	</Card.Content>
</Card.Root>
