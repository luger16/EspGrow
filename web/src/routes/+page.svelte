<script lang="ts">
	import PageHeader from "$lib/components/page-header.svelte";
	import SensorCard from "$lib/components/sensor-card.svelte";
	import DeviceCard from "$lib/components/device-card.svelte";
	import { sensors, sensorReadings } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import ThermometerIcon from "@lucide/svelte/icons/thermometer";
	import PowerIcon from "@lucide/svelte/icons/power";
</script>

<PageHeader title="Dashboard" />
<div class="flex flex-1 flex-col gap-6 p-4 pt-0">
	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Sensors</h2>
		{#if sensors.length === 0}
			<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
				<ThermometerIcon class="size-8 text-muted-foreground/50" />
				<p class="mt-3 text-sm font-medium">No sensors</p>
				<p class="mt-1 text-xs text-muted-foreground">Add sensors in Settings to start monitoring</p>
			</div>
		{:else}
			<div class="grid grid-cols-2 gap-3 md:grid-cols-3 lg:grid-cols-4">
				{#each sensors as sensor (sensor.id)}
					<SensorCard {sensor} reading={sensorReadings[sensor.id]} />
				{/each}
			</div>
		{/if}
	</section>

	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Devices</h2>
		{#if devices.length === 0}
			<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
				<PowerIcon class="size-8 text-muted-foreground/50" />
				<p class="mt-3 text-sm font-medium">No devices</p>
				<p class="mt-1 text-xs text-muted-foreground">Add devices in Settings to control equipment</p>
			</div>
		{:else}
			<div class="grid grid-cols-1 gap-3 md:grid-cols-2 lg:grid-cols-3">
				{#each devices as device (device.id)}
					<DeviceCard {device} />
				{/each}
			</div>
		{/if}
	</section>
</div>
