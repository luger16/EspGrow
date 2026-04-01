<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import * as Select from "$lib/components/ui/select/index.js";
	import * as Tabs from "$lib/components/ui/tabs/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { Switch } from "$lib/components/ui/switch/index.js";
	import CircularTimePicker from "$lib/components/circular-time-picker.svelte";
	import { setDeviceMode, deleteDeviceMode, getDeviceMode } from "$lib/stores/device-modes.svelte";
	import { sensors } from "$lib/stores/sensors.svelte";
	import type { Device, DeviceMode, AutoTrigger, DeviceModeConfig } from "$lib/types";
	import PlusIcon from "@lucide/svelte/icons/plus";
	import TrashIcon from "@lucide/svelte/icons/trash-2";

	type Props = {
		device: Device;
		open: boolean;
		onOpenChange: (open: boolean) => void;
	};
	let { device, open = $bindable(), onOpenChange }: Props = $props();

	let mode = $state<DeviceMode>("off");
	let triggers = $state<AutoTrigger[]>([]);
	let cycleOnMin = $state(1);
	let cycleOffMin = $state(1);
	let cycleDayOnly = $state(false);
	let scheduleStart = $state("06:00");
	let scheduleEnd = $state("22:00");

	const modeOptions: { value: DeviceMode; label: string }[] = [
		{ value: "off", label: "Off" },
		{ value: "on", label: "On" },
		{ value: "auto", label: "Auto" },
		{ value: "cycle", label: "Cycle" },
		{ value: "schedule", label: "Schedule" },
	];

	const sensorTypeOptions = $derived(
		[...new Set(sensors.filter((s) => ["temperature", "humidity", "co2", "vpd"].includes(s.type)).map((s) => s.type))].map((t) => ({
			value: t,
			label: t.charAt(0).toUpperCase() + t.slice(1),
		})),
	);

	$effect(() => {
		if (open) {
			const existing = getDeviceMode(device.id);
			if (existing) {
				mode = existing.mode;
				triggers = existing.triggers.map((t) => ({ ...t }));
				cycleOnMin = Math.round(existing.cycle.onDurationSec / 60) || 1;
				cycleOffMin = Math.round(existing.cycle.offDurationSec / 60) || 1;
				cycleDayOnly = existing.cycle.dayOnly;
				scheduleStart = existing.schedule.startTime;
				scheduleEnd = existing.schedule.endTime;
			} else {
				mode = "off";
				triggers = [];
				cycleOnMin = 1;
				cycleOffMin = 1;
				cycleDayOnly = false;
				scheduleStart = "06:00";
				scheduleEnd = "22:00";
			}
		}
	});

	function addTrigger(): void {
		if (triggers.length >= 3) return;
		triggers.push({
			sensorType: sensorTypeOptions[0]?.value ?? "temperature",
			dayThreshold: 25,
			nightThreshold: 22,
			deadzone: 1,
			triggerAbove: true,
		});
	}

	function removeTrigger(index: number): void {
		triggers.splice(index, 1);
	}

	function handleSave(): void {
		const config: DeviceModeConfig = {
			deviceId: device.id,
			mode,
			triggers: mode === "auto" ? triggers : [],
			cycle: {
				onDurationSec: Math.max(60, cycleOnMin * 60),
				offDurationSec: Math.max(60, cycleOffMin * 60),
				dayOnly: cycleDayOnly,
			},
			schedule: {
				startTime: scheduleStart,
				endTime: scheduleEnd,
			},
		};
		setDeviceMode(config);
		onOpenChange(false);
	}

	function handleRemove(): void {
		deleteDeviceMode(device.id);
		onOpenChange(false);
	}
</script>

<Dialog.Root {open} {onOpenChange}>
	<Dialog.Content class="max-h-[85vh] overflow-y-auto sm:max-w-lg">
		<Dialog.Header>
			<Dialog.Title>{device.name}</Dialog.Title>
			<Dialog.Description>Configure how this device is controlled.</Dialog.Description>
		</Dialog.Header>
		<div class="grid gap-4 py-4">
			<div class="grid gap-2">
				<Label>Mode</Label>
				<Tabs.Root value={mode} onValueChange={(v) => v && (mode = v as DeviceMode)}>
					<Tabs.List class="w-full">
						{#each modeOptions as option (option.value)}
							<Tabs.Trigger value={option.value}>{option.label}</Tabs.Trigger>
						{/each}
					</Tabs.List>
				</Tabs.Root>
			</div>

			{#if mode === "auto"}
				<div class="grid gap-3">
					<div class="flex items-center justify-between">
						<Label>Triggers</Label>
						{#if triggers.length < 3}
							<Button variant="outline" size="sm" onclick={addTrigger}>
								<PlusIcon class="mr-1 size-3.5" />
								Add
							</Button>
						{/if}
					</div>
					{#if triggers.length === 0}
						<p class="text-xs text-muted-foreground">Add at least one trigger. Any trigger matching will turn the device on (OR logic).</p>
					{/if}
					{#each triggers as trigger, i (i)}
						<div class="rounded-lg border p-3">
							<div class="mb-3 flex items-center justify-between">
								<span class="text-xs font-medium text-muted-foreground">Trigger {i + 1}</span>
								<Button variant="ghost" size="icon" class="size-7" onclick={() => removeTrigger(i)}>
									<TrashIcon class="size-3.5 text-muted-foreground" />
								</Button>
							</div>
							<div class="grid gap-3">
								<div class="grid gap-1.5">
									<Label class="text-xs">Sensor Type</Label>
									<Select.Root type="single" value={trigger.sensorType} onValueChange={(v) => v && (trigger.sensorType = v)}>
										<Select.Trigger class="h-8 text-xs">
											<span>{sensorTypeOptions.find((o) => o.value === trigger.sensorType)?.label ?? trigger.sensorType}</span>
										</Select.Trigger>
										<Select.Content>
											{#each sensorTypeOptions as option (option.value)}
												<Select.Item value={option.value}>{option.label}</Select.Item>
											{/each}
										</Select.Content>
									</Select.Root>
								</div>
								<div class="flex items-center gap-2">
									<Label class="min-w-fit text-xs">Turn on when</Label>
									<Select.Root
										type="single"
										value={trigger.triggerAbove ? "above" : "below"}
										onValueChange={(v) => (trigger.triggerAbove = v === "above")}
									>
										<Select.Trigger class="h-8 text-xs">
											<span>{trigger.triggerAbove ? "above" : "below"}</span>
										</Select.Trigger>
										<Select.Content>
											<Select.Item value="above">above</Select.Item>
											<Select.Item value="below">below</Select.Item>
										</Select.Content>
									</Select.Root>
								</div>
								<div class="grid grid-cols-2 gap-2">
									<div class="grid gap-1.5">
										<Label class="text-xs">Day threshold</Label>
										<Input type="number" step="0.1" class="h-8 text-xs" bind:value={trigger.dayThreshold} />
									</div>
									<div class="grid gap-1.5">
										<Label class="text-xs">Night threshold</Label>
										<Input type="number" step="0.1" class="h-8 text-xs" bind:value={trigger.nightThreshold} />
									</div>
								</div>
								<div class="grid gap-1.5">
									<Label class="text-xs">Deadzone</Label>
									<Input type="number" step="0.1" min="0" class="h-8 text-xs" bind:value={trigger.deadzone} />
									<p class="text-[10px] text-muted-foreground">
										Distance from threshold before switching on or off
									</p>
								</div>
							</div>
						</div>
					{/each}
				</div>
			{/if}

			{#if mode === "cycle"}
				<div class="grid gap-3">
					<div class="grid grid-cols-2 gap-2">
						<div class="grid gap-1.5">
							<Label class="text-xs">On duration (min)</Label>
							<Input type="number" min="1" class="h-8 text-xs" bind:value={cycleOnMin} />
						</div>
						<div class="grid gap-1.5">
							<Label class="text-xs">Off duration (min)</Label>
							<Input type="number" min="1" class="h-8 text-xs" bind:value={cycleOffMin} />
						</div>
					</div>
					<div class="flex items-center gap-2">
						<Switch bind:checked={cycleDayOnly} />
						<Label class="text-xs font-normal">Only cycle during daytime</Label>
					</div>
				</div>
			{/if}

		{#if mode === "schedule"}
			<CircularTimePicker bind:startTime={scheduleStart} bind:endTime={scheduleEnd} />
		{/if}
		</div>
		<Dialog.Footer class="flex-col gap-2 sm:flex-row sm:justify-between">
			{#if getDeviceMode(device.id)}
				<Button type="button" variant="destructive" size="sm" onclick={handleRemove}>Remove Mode</Button>
			{:else}
				<div></div>
			{/if}
			<Button onclick={handleSave}>Save</Button>
		</Dialog.Footer>
	</Dialog.Content>
</Dialog.Root>
