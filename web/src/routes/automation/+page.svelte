<script lang="ts">
	import PageHeader from "$lib/components/page-header.svelte";
	import DeviceModeConfigModal from "$lib/components/device-mode-config.svelte";
	import { Badge } from "$lib/components/ui/badge/index.js";
	import { deviceModes, dayNightConfig, setDayNightConfig } from "$lib/stores/device-modes.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import { sensors } from "$lib/stores/sensors.svelte";
	import { Switch } from "$lib/components/ui/switch/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { deviceIcons } from "$lib/icons";
	import type { Device, DeviceMode } from "$lib/types";
	import ZapIcon from "@lucide/svelte/icons/zap";
	import SunMoonIcon from "@lucide/svelte/icons/sun-moon";

	let editingDevice = $state<Device | null>(null);

	function getDeviceMode(deviceId: string): DeviceMode | undefined {
		return deviceModes.find((m) => m.deviceId === deviceId)?.mode;
	}

	function getModeLabel(mode: DeviceMode | undefined): string {
		if (!mode) return "No mode";
		const labels: Record<DeviceMode, string> = {
			off: "Off",
			on: "Always On",
			auto: "Auto",
			cycle: "Cycle",
			schedule: "Schedule",
		};
		return labels[mode];
	}

	function getModeBadgeVariant(mode: DeviceMode | undefined): "default" | "secondary" | "outline" | "destructive" {
		if (!mode || mode === "off") return "outline";
		return "secondary";
	}

	function getModeDescription(deviceId: string): string {
		const config = deviceModes.find((m) => m.deviceId === deviceId);
		if (!config) return "No automation configured";

		switch (config.mode) {
			case "off":
				return "Device will stay off";
			case "on":
				return "Device will stay on";
			case "auto": {
				if (config.triggers.length === 0) return "No triggers configured";
				const descriptions = config.triggers.map((t) => {
					const direction = t.triggerAbove ? ">" : "<";
					return `${t.sensorType} ${direction} ${t.dayThreshold}/${t.nightThreshold}`;
				});
				return descriptions.join(" or ");
			}
			case "cycle": {
				const dayLabel = config.cycle.dayOnly ? " (day only)" : "";
				return `${config.cycle.onDurationSec}s on / ${config.cycle.offDurationSec}s off${dayLabel}`;
			}
			case "schedule":
				return `${config.schedule.startTime} – ${config.schedule.endTime}`;
			default:
				return "";
		}
	}

	function handleDayNightToggle(useSchedule: boolean): void {
		setDayNightConfig({ ...dayNightConfig, useSchedule });
	}

	function saveDayNightTimes(): void {
		setDayNightConfig({
			useSchedule: dayNightConfig.useSchedule,
			dayStartTime: dayNightConfig.dayStartTime,
			nightStartTime: dayNightConfig.nightStartTime,
			lightThreshold: dayNightConfig.lightThreshold,
			lightHysteresis: dayNightConfig.lightHysteresis,
		});
	}
</script>

<PageHeader title="Automation" />
<div class="flex flex-1 flex-col gap-6 p-4 pt-0">
	<section>
		<div class="mb-3 flex items-center gap-2">
			<SunMoonIcon class="size-4 text-muted-foreground" />
			<h2 class="text-sm font-medium text-muted-foreground">Day / Night Detection</h2>
		</div>
		<div class="divide-y divide-border rounded-lg border">
			<div class="flex items-center justify-between p-3">
				<div>
					<p class="text-sm font-medium">
						{dayNightConfig.isDaytime ? "☀️ Daytime" : "🌙 Nighttime"}
					</p>
					<p class="text-xs text-muted-foreground">
						{dayNightConfig.useSchedule ? "Based on schedule" : "Based on light sensor"}
					</p>
				</div>
				<div class="flex items-center gap-2">
					<Label class="text-xs text-muted-foreground">Use schedule</Label>
					<Switch
						checked={dayNightConfig.useSchedule}
						onCheckedChange={handleDayNightToggle}
					/>
				</div>
			</div>
			{#if dayNightConfig.useSchedule}
				<div class="flex items-center gap-4 p-3">
					<div class="grid gap-1">
						<Label class="text-xs">Day starts</Label>
						<Input
							type="time"
							class="h-8 w-28 text-xs"
							value={dayNightConfig.dayStartTime}
							onchange={(e) => {
								dayNightConfig.dayStartTime = e.currentTarget.value;
								saveDayNightTimes();
							}}
						/>
					</div>
					<div class="grid gap-1">
						<Label class="text-xs">Night starts</Label>
						<Input
							type="time"
							class="h-8 w-28 text-xs"
							value={dayNightConfig.nightStartTime}
							onchange={(e) => {
								dayNightConfig.nightStartTime = e.currentTarget.value;
								saveDayNightTimes();
							}}
						/>
					</div>
				</div>
			{:else}
				<div class="flex items-center gap-4 p-3">
					<div class="grid gap-1">
						<Label class="text-xs">Light threshold (PPFD)</Label>
						<Input
							type="number"
							class="h-8 w-28 text-xs"
							value={dayNightConfig.lightThreshold}
							onchange={(e) => {
								dayNightConfig.lightThreshold = Number(e.currentTarget.value);
								saveDayNightTimes();
							}}
						/>
					</div>
					<div class="grid gap-1">
						<Label class="text-xs">Hysteresis</Label>
						<Input
							type="number"
							class="h-8 w-28 text-xs"
							value={dayNightConfig.lightHysteresis}
							onchange={(e) => {
								dayNightConfig.lightHysteresis = Number(e.currentTarget.value);
								saveDayNightTimes();
							}}
						/>
					</div>
				</div>
			{/if}
		</div>
	</section>

	<section>
		<div class="mb-3 flex items-center gap-2">
			<ZapIcon class="size-4 text-muted-foreground" />
			<h2 class="text-sm font-medium text-muted-foreground">Device Modes</h2>
		</div>
		{#if devices.length === 0}
			<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
				<ZapIcon class="size-8 text-muted-foreground/50" />
				<p class="mt-3 text-sm font-medium">No devices</p>
				<p class="mt-1 text-xs text-muted-foreground">
					Add devices in <a href="/settings" class="underline">Settings</a> first
				</p>
			</div>
		{:else}
			<div class="divide-y divide-border rounded-lg border">
				{#each devices as device (device.id)}
					{@const Icon = deviceIcons[device.type]}
					{@const mode = getDeviceMode(device.id)}
					<button
						class="flex w-full items-center gap-3 p-3 text-left transition-colors hover:bg-muted/50"
						onclick={() => (editingDevice = device)}
					>
						<div class="flex size-9 items-center justify-center rounded-md bg-muted">
							<Icon class="size-4 text-muted-foreground" />
						</div>
						<div class="min-w-0 flex-1">
							<div class="flex items-center gap-2">
								<p class="text-sm font-medium">{device.name}</p>
								<Badge variant={getModeBadgeVariant(mode)} class="px-1.5 py-0 text-[10px]">
									{getModeLabel(mode)}
								</Badge>
							</div>
							<p class="truncate text-xs text-muted-foreground">
								{getModeDescription(device.id)}
							</p>
						</div>
					</button>
				{/each}
			</div>
		{/if}
	</section>
</div>

{#if editingDevice}
	<DeviceModeConfigModal
		device={editingDevice}
		open={!!editingDevice}
		onOpenChange={(open) => !open && (editingDevice = null)}
	/>
{/if}
