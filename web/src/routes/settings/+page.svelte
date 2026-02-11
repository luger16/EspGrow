<script lang="ts">
	import PageHeader from "$lib/components/page-header.svelte";
	import AddSensorModal from "$lib/components/add-sensor-modal.svelte";
	import AddDeviceModal from "$lib/components/add-device-modal.svelte";
	import EditSensorModal from "$lib/components/edit-sensor-modal.svelte";
	import EditDeviceModal from "$lib/components/edit-device-modal.svelte";
	import * as Select from "$lib/components/ui/select/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { sensors } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import { settings, updateSettings, type Theme, type TemperatureUnit } from "$lib/stores/settings.svelte";
	import { sensorIcons, deviceIcons } from "$lib/icons";
	import PencilIcon from "@lucide/svelte/icons/pencil";
	import ThermometerIcon from "@lucide/svelte/icons/thermometer";
	import FanIcon from "@lucide/svelte/icons/fan";
	import type { Sensor, Device } from "$lib/types";

	const hardwareLabels: Record<Sensor["hardwareType"], string> = {
		sht3x: "SHT3x",
		sht4x: "SHT4x",
		scd4x: "SCD4x",
		as7341: "AS7341",
		calculated: "Calculated",
	};

	const controlMethodLabels: Record<Device["controlMethod"], string> = {
		shelly_gen1: "Shelly Gen1",
		shelly_gen2: "Shelly Gen2/Plus",
		tasmota: "Tasmota",
	};

	const themeOptions: { value: Theme; label: string }[] = [
		{ value: "system", label: "System" },
		{ value: "light", label: "Light" },
		{ value: "dark", label: "Dark" },
	];

	const temperatureUnitOptions: { value: TemperatureUnit; label: string }[] = [
		{ value: "celsius", label: "Celsius (°C)" },
		{ value: "fahrenheit", label: "Fahrenheit (°F)" },
	];

	let editingSensorId = $state<string | null>(null);
	let editingDeviceId = $state<string | null>(null);

	const editingSensor = $derived(sensors.find((s) => s.id === editingSensorId));
	const editingDevice = $derived(devices.find((d) => d.id === editingDeviceId));
</script>

<PageHeader title="Settings" />
<div class="flex flex-1 flex-col gap-6 p-4 pt-0">
	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Preferences</h2>
		<div class="divide-y divide-border rounded-lg border">
			<div class="flex items-center justify-between p-3">
				<Label>Temperature Unit</Label>
				<Select.Root
					type="single"
					value={settings.temperatureUnit}
					onValueChange={(v) => v && updateSettings({ temperatureUnit: v as TemperatureUnit })}
				>
					<Select.Trigger class="w-44">
						<span>{temperatureUnitOptions.find((o) => o.value === settings.temperatureUnit)?.label}</span>
					</Select.Trigger>
					<Select.Content>
						{#each temperatureUnitOptions as option (option.value)}
							<Select.Item value={option.value}>{option.label}</Select.Item>
						{/each}
					</Select.Content>
				</Select.Root>
			</div>
			<div class="flex items-center justify-between p-3">
				<Label>Theme</Label>
				<Select.Root
					type="single"
					value={settings.theme}
					onValueChange={(v) => v && updateSettings({ theme: v as Theme })}
				>
					<Select.Trigger class="w-44">
						<span>{themeOptions.find((o) => o.value === settings.theme)?.label}</span>
					</Select.Trigger>
					<Select.Content>
						{#each themeOptions as option (option.value)}
							<Select.Item value={option.value}>{option.label}</Select.Item>
						{/each}
					</Select.Content>
				</Select.Root>
			</div>
		</div>
	</section>

	<section>
		<div class="mb-3 flex items-center justify-between">
			<h2 class="text-sm font-medium text-muted-foreground">Sensors</h2>
			<AddSensorModal />
		</div>
		{#if sensors.length === 0}
			<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
				<ThermometerIcon class="size-8 text-muted-foreground/50" />
				<p class="mt-3 text-sm font-medium">No sensors</p>
				<p class="mt-1 text-xs text-muted-foreground">Add a sensor to start monitoring your grow tent</p>
			</div>
		{:else}
			<div class="divide-y divide-border rounded-lg border">
				{#each sensors as sensor (sensor.id)}
					{@const SensorIcon = sensorIcons[sensor.type]}
					<div class="flex items-center gap-3 p-3">
						<div class="flex size-9 items-center justify-center rounded-md bg-muted">
							<SensorIcon class="size-4 text-muted-foreground" />
						</div>
						<div class="flex-1">
							<p class="text-sm font-medium">{sensor.name}</p>
							<p class="text-xs text-muted-foreground">
								{hardwareLabels[sensor.hardwareType]}{sensor.address ? ` · ${sensor.address}` : ""}
							</p>
						</div>
						<Button variant="ghost" size="icon" onclick={() => (editingSensorId = sensor.id)}>
							<PencilIcon class="size-4 text-muted-foreground" />
						</Button>
					</div>
				{/each}
			</div>
		{/if}
	</section>

	<section>
		<div class="mb-3 flex items-center justify-between">
			<h2 class="text-sm font-medium text-muted-foreground">Devices</h2>
			<AddDeviceModal />
		</div>
		{#if devices.length === 0}
			<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
				<FanIcon class="size-8 text-muted-foreground/50" />
				<p class="mt-3 text-sm font-medium">No devices</p>
				<p class="mt-1 text-xs text-muted-foreground">Add a device to control fans, lights, or other equipment</p>
			</div>
		{:else}
			<div class="divide-y divide-border rounded-lg border">
				{#each devices as device (device.id)}
					{@const DeviceIcon = deviceIcons[device.type]}
					<div class="flex items-center gap-3 p-3">
						<div class="flex size-9 items-center justify-center rounded-md bg-muted">
							<DeviceIcon class="size-4 text-muted-foreground" />
						</div>
						<div class="flex-1">
							<p class="text-sm font-medium">{device.name}</p>
							<p class="text-xs text-muted-foreground">
								{controlMethodLabels[device.controlMethod]}{device.ipAddress ? ` · ${device.ipAddress}` : ""}
							</p>
						</div>
						<Button variant="ghost" size="icon" onclick={() => (editingDeviceId = device.id)}>
							<PencilIcon class="size-4 text-muted-foreground" />
						</Button>
					</div>
				{/each}
			</div>
		{/if}
	</section>
</div>

{#if editingSensor}
	<EditSensorModal
		sensor={editingSensor}
		open={!!editingSensorId}
		onOpenChange={(open) => !open && (editingSensorId = null)}
	/>
{/if}

{#if editingDevice}
	<EditDeviceModal
		device={editingDevice}
		open={!!editingDeviceId}
		onOpenChange={(open) => !open && (editingDeviceId = null)}
	/>
{/if}
