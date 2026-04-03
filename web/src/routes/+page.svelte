<script lang="ts">
	import PageHeader from "$lib/components/page-header.svelte";
	import SensorCard from "$lib/components/sensor-card.svelte";
	import DeviceCard from "$lib/components/device-card.svelte";
	import AnalyticsChart from "$lib/components/analytics-chart.svelte";

	import ClimateOverviewCard from "$lib/components/climate-overview-card.svelte";
	import EventHistoryModal from "$lib/components/event-history-modal.svelte";
	import VpdZoneChart from "$lib/components/vpd-zone-chart.svelte";
	import SpectrumChart from "$lib/components/spectrum-chart.svelte";
	import * as Dialog from "$lib/components/ui/dialog/index.js";

	import { sensors, sensorReadings } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import { settings, reconcileOrder } from "$lib/stores/settings.svelte";
	import ThermometerIcon from "@lucide/svelte/icons/thermometer";
	import PowerIcon from "@lucide/svelte/icons/power";

	let alertHistoryOpen = $state(false);
	let vpdChartOpen = $state(false);
	let spectrumChartOpen = $state(false);

	const visibleSensors = $derived.by(() => {
		const visible = sensors.filter((s) => !settings.hiddenSensors.includes(s.id));
		const order = reconcileOrder(settings.sensorOrder, visible.map((s) => s.id));
		return [...visible].sort((a, b) => order.indexOf(a.id) - order.indexOf(b.id));
	});

	const visibleDevices = $derived.by(() => {
		const visible = devices.filter((d) => !settings.hiddenDevices.includes(d.id));
		const order = reconcileOrder(settings.deviceOrder, visible.map((d) => d.id));
		return [...visible].sort((a, b) => order.indexOf(a.id) - order.indexOf(b.id));
	});
</script>

<PageHeader title="Dashboard" />
<div class="@container flex flex-1 flex-col gap-6 p-4 pt-0">
	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Overview</h2>
		<ClimateOverviewCard onhistoryclick={() => (alertHistoryOpen = true)} />
	</section>

	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Sensors</h2>
		{#if visibleSensors.length === 0}
			<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
				<ThermometerIcon class="size-8 text-muted-foreground/50" />
				<p class="mt-3 text-sm font-medium">No sensors</p>
				<p class="mt-1 text-xs text-muted-foreground">Add sensors in <a href="/settings/sensors" class="underline">Settings</a> to start monitoring</p>
			</div>
		{:else}
			<div class="grid grid-cols-2 gap-3 @lg:grid-cols-3 @2xl:grid-cols-4">
				{#each visibleSensors as sensor (sensor.id)}
				<SensorCard
					{sensor}
					reading={sensorReadings[sensor.id]}
					onvpdclick={sensor.type === "vpd" ? () => (vpdChartOpen = true) : undefined}
					onspectrumclick={sensor.type === "light" ? () => (spectrumChartOpen = true) : undefined}
				/>
				{/each}
			</div>
		{/if}
	</section>

	{#if visibleSensors.length > 0}
		<section>
			<AnalyticsChart />
		</section>
	{/if}

	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Devices</h2>
		{#if visibleDevices.length === 0}
			<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
				<PowerIcon class="size-8 text-muted-foreground/50" />
				<p class="mt-3 text-sm font-medium">No devices</p>
				<p class="mt-1 text-xs text-muted-foreground">Add devices in <a href="/settings/devices" class="underline">Settings</a> to control equipment</p>
			</div>
		{:else}
			<div class="grid grid-cols-1 gap-3 md:grid-cols-2 lg:grid-cols-3">
				{#each visibleDevices as device (device.id)}
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

<Dialog.Root bind:open={spectrumChartOpen}>
	<Dialog.Content class="max-w-lg gap-3 p-0 sm:max-w-2xl">
		<SpectrumChart />
	</Dialog.Content>
</Dialog.Root>
