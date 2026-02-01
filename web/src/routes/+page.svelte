<script lang="ts">
	import PageHeader from "$lib/components/page-header.svelte";
	import SensorCard from "$lib/components/sensor-card.svelte";
	import DeviceCard from "$lib/components/device-card.svelte";
	import { sensors, sensorReadings } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
</script>

<PageHeader title="Dashboard" />
<div class="flex flex-1 flex-col gap-6 p-4 pt-0">
	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Sensors</h2>
		<div class="grid gap-3" style="grid-template-columns: repeat(auto-fill, minmax(300px, 1fr))">
			{#each sensors as sensor (sensor.id)}
				<SensorCard {sensor} reading={sensorReadings[sensor.id]} />
			{/each}
		</div>
	</section>

	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Devices</h2>
		<div class="grid gap-3" style="grid-template-columns: repeat(auto-fill, minmax(300px, 1fr))">
			{#each devices as device (device.id)}
				<DeviceCard {device} />
			{/each}
		</div>
	</section>
</div>
