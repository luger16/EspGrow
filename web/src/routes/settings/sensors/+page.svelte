<script lang="ts">
	import PageHeader from "$lib/components/page-header.svelte";
	import AddSensorModal from "$lib/components/add-sensor-modal.svelte";
	import EditSensorModal from "$lib/components/edit-sensor-modal.svelte";
	import { Button } from "$lib/components/ui/button/index.js";

	import { sensors } from "$lib/stores/sensors.svelte";
	import { settings, toggleSensorVisibility, moveSensor } from "$lib/stores/settings.svelte";
	import { sensorIcons } from "$lib/icons";
	import type { Sensor } from "$lib/types";
	import PencilIcon from "@lucide/svelte/icons/pencil";
	import ChevronUpIcon from "@lucide/svelte/icons/chevron-up";
	import ChevronDownIcon from "@lucide/svelte/icons/chevron-down";
	import EyeIcon from "@lucide/svelte/icons/eye";
	import EyeOffIcon from "@lucide/svelte/icons/eye-off";
	import ThermometerIcon from "@lucide/svelte/icons/thermometer";
	import ArrowLeftIcon from "@lucide/svelte/icons/arrow-left";

	const hardwareLabels: Record<Sensor["hardwareType"], string> = {
		sht3x: "SHT3x",
		sht4x: "SHT4x",
		scd4x: "SCD4x",
		as7341: "AS7341",
		calculated: "Calculated",
	};

	let editingSensorId = $state<string | null>(null);
	const editingSensor = $derived(sensors.find((s) => s.id === editingSensorId));

	const orderedSensors = $derived.by(() => {
		if (settings.sensorOrder.length === 0) return sensors;
		return [...sensors].sort((a, b) => {
			const ai = settings.sensorOrder.indexOf(a.id);
			const bi = settings.sensorOrder.indexOf(b.id);
			if (ai === -1 && bi === -1) return 0;
			if (ai === -1) return 1;
			if (bi === -1) return -1;
			return ai - bi;
		});
	});

	const allSensorIds = $derived(orderedSensors.map((s) => s.id));

	function isHidden(sensorId: string): boolean {
		return settings.hiddenSensors.includes(sensorId);
	}
</script>

<PageHeader title="Sensors" />
<div class="flex flex-1 flex-col gap-6 p-4 pt-0">
	<div class="flex items-center gap-2">
		<Button variant="ghost" size="icon" href="/settings">
			<ArrowLeftIcon class="size-4" />
		</Button>
		<h2 class="text-sm font-medium text-muted-foreground">Manage Sensors</h2>
		<div class="ml-auto">
			<AddSensorModal />
		</div>
	</div>

	{#if sensors.length === 0}
		<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
			<ThermometerIcon class="size-8 text-muted-foreground/50" />
			<p class="mt-3 text-sm font-medium">No sensors</p>
			<p class="mt-1 text-xs text-muted-foreground">Add a sensor to start monitoring your grow tent</p>
		</div>
	{:else}
		<div class="divide-y divide-border rounded-lg border">
			{#each orderedSensors as sensor, i (sensor.id)}
				{@const SensorIcon = sensorIcons[sensor.type]}
				{@const hidden = isHidden(sensor.id)}
				<div class="flex items-center gap-3 p-3 {hidden ? 'opacity-50' : ''}">
					<div class="flex flex-col gap-0.5">
						<Button
							variant="ghost"
							size="icon"
							class="size-6"
							disabled={i === 0}
							onclick={() => moveSensor(sensor.id, "up", allSensorIds)}
						>
							<ChevronUpIcon class="size-3.5" />
						</Button>
						<Button
							variant="ghost"
							size="icon"
							class="size-6"
							disabled={i === orderedSensors.length - 1}
							onclick={() => moveSensor(sensor.id, "down", allSensorIds)}
						>
							<ChevronDownIcon class="size-3.5" />
						</Button>
					</div>
					<div class="flex size-9 items-center justify-center rounded-md bg-muted">
						<SensorIcon class="size-4 text-muted-foreground" />
					</div>
					<div class="flex-1">
						<p class="text-sm font-medium">{sensor.name}</p>
						<p class="text-xs text-muted-foreground">
							{hardwareLabels[sensor.hardwareType]}{sensor.address ? ` · ${sensor.address}` : ""}
						</p>
					</div>
					<Button
						variant="ghost"
						size="icon"
						onclick={() => toggleSensorVisibility(sensor.id)}
						title={hidden ? "Show on dashboard" : "Hide from dashboard"}
					>
						{#if hidden}
							<EyeOffIcon class="size-4 text-muted-foreground" />
						{:else}
							<EyeIcon class="size-4 text-muted-foreground" />
						{/if}
					</Button>
					<Button variant="ghost" size="icon" onclick={() => (editingSensorId = sensor.id)}>
						<PencilIcon class="size-4 text-muted-foreground" />
					</Button>
				</div>
			{/each}
		</div>
		<p class="text-xs text-muted-foreground">
			Use arrows to reorder sensors on the dashboard. Click the eye icon to show or hide a sensor.
		</p>
	{/if}
</div>

{#if editingSensor}
	<EditSensorModal
		sensor={editingSensor}
		open={!!editingSensorId}
		onOpenChange={(open) => !open && (editingSensorId = null)}
	/>
{/if}
