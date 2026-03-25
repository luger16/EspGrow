<script lang="ts">
	import PageHeader from "$lib/components/page-header.svelte";
	import SensorCard from "$lib/components/sensor-card.svelte";
	import DeviceCard from "$lib/components/device-card.svelte";
	import AnalyticsChart from "$lib/components/analytics-chart.svelte";

	import ClimateOverviewCard from "$lib/components/climate-overview-card.svelte";
	import EventHistoryModal from "$lib/components/event-history-modal.svelte";
	import VpdZoneChart from "$lib/components/vpd-zone-chart.svelte";
	import * as Dialog from "$lib/components/ui/dialog/index.js";

	import { sensors, sensorReadings } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import ThermometerIcon from "@lucide/svelte/icons/thermometer";
	import PowerIcon from "@lucide/svelte/icons/power";

	let alertHistoryOpen = $state(false);
	let vpdChartOpen = $state(false);
</script>

<PageHeader title="Dashboard" />
<div class="flex flex-1 flex-col gap-6 p-4 pt-0">
	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Overview</h2>
		<ClimateOverviewCard onhistoryclick={() => (alertHistoryOpen = true)} />
	</section>

	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Sensors</h2>
		{#if sensors.length === 0}
			<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
				<ThermometerIcon class="size-8 text-muted-foreground/50" />
				<p class="mt-3 text-sm font-medium">No sensors</p>
				<p class="mt-1 text-xs text-muted-foreground">Add sensors in <a href="/settings" class="underline">Settings</a> to start monitoring</p>
			</div>
		{:else}
			<div class="grid grid-cols-2 gap-3 md:grid-cols-3 lg:grid-cols-4">
				{#each sensors as sensor (sensor.id)}
					<SensorCard
						{sensor}
						reading={sensorReadings[sensor.id]}
						onvpdclick={sensor.type === "vpd" ? () => (vpdChartOpen = true) : undefined}
					/>
				{/each}
			</div>
		{/if}
	</section>

	{#if sensors.length > 0}
		<section>
			<AnalyticsChart />
		</section>
	{/if}

	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Devices</h2>
		{#if devices.length === 0}
			<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
				<PowerIcon class="size-8 text-muted-foreground/50" />
				<p class="mt-3 text-sm font-medium">No devices</p>
				<p class="mt-1 text-xs text-muted-foreground">Add devices in <a href="/settings" class="underline">Settings</a> to control equipment</p>
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

<EventHistoryModal
	bind:open={alertHistoryOpen}
	onOpenChange={(open: boolean) => (alertHistoryOpen = open)}
/>

<Dialog.Root bind:open={vpdChartOpen}>
	<Dialog.Content class="max-w-lg gap-3 p-0 sm:max-w-2xl">
		<VpdZoneChart />
	</Dialog.Content>
</Dialog.Root>
