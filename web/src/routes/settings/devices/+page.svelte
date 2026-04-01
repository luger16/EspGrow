<script lang="ts">
	import PageHeader from "$lib/components/page-header.svelte";
	import AddDeviceModal from "$lib/components/add-device-modal.svelte";
	import EditDeviceModal from "$lib/components/edit-device-modal.svelte";
	import { Button } from "$lib/components/ui/button/index.js";

	import { devices } from "$lib/stores/devices.svelte";
	import { settings, toggleDeviceVisibility, moveDevice } from "$lib/stores/settings.svelte";
	import { deviceIcons } from "$lib/icons";
	import type { Device } from "$lib/types";
	import PencilIcon from "@lucide/svelte/icons/pencil";
	import ChevronUpIcon from "@lucide/svelte/icons/chevron-up";
	import ChevronDownIcon from "@lucide/svelte/icons/chevron-down";
	import EyeIcon from "@lucide/svelte/icons/eye";
	import EyeOffIcon from "@lucide/svelte/icons/eye-off";
	import PowerIcon from "@lucide/svelte/icons/power";
	import ArrowLeftIcon from "@lucide/svelte/icons/arrow-left";

	const controlMethodLabels: Record<Device["controlMethod"], string> = {
		shelly_gen1: "Shelly Gen1",
		shelly_gen2: "Shelly Gen2/Plus",
		tasmota: "Tasmota",
	};

	let editingDeviceId = $state<string | null>(null);
	const editingDevice = $derived(devices.find((d) => d.id === editingDeviceId));

	const orderedDevices = $derived.by(() => {
		if (settings.deviceOrder.length === 0) return devices;
		return [...devices].sort((a, b) => {
			const ai = settings.deviceOrder.indexOf(a.id);
			const bi = settings.deviceOrder.indexOf(b.id);
			if (ai === -1 && bi === -1) return 0;
			if (ai === -1) return 1;
			if (bi === -1) return -1;
			return ai - bi;
		});
	});

	const allDeviceIds = $derived(orderedDevices.map((d) => d.id));

	function isHidden(deviceId: string): boolean {
		return settings.hiddenDevices.includes(deviceId);
	}
</script>

<PageHeader title="Devices" />
<div class="flex flex-1 flex-col gap-6 p-4 pt-0">
	<div class="flex items-center gap-2">
		<Button variant="ghost" size="icon" href="/settings">
			<ArrowLeftIcon class="size-4" />
		</Button>
		<h2 class="text-sm font-medium text-muted-foreground">Manage Devices</h2>
		<div class="ml-auto">
			<AddDeviceModal />
		</div>
	</div>

	{#if devices.length === 0}
		<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
			<PowerIcon class="size-8 text-muted-foreground/50" />
			<p class="mt-3 text-sm font-medium">No devices</p>
			<p class="mt-1 text-xs text-muted-foreground">Add a device to control fans, lights, or other equipment</p>
		</div>
	{:else}
		<div class="divide-y divide-border rounded-lg border">
			{#each orderedDevices as device, i (device.id)}
				{@const DeviceIcon = deviceIcons[device.type]}
				{@const hidden = isHidden(device.id)}
				<div class="flex items-center gap-3 p-3 {hidden ? 'opacity-50' : ''}">
					<div class="flex flex-col gap-0.5">
						<Button
							variant="ghost"
							size="icon"
							class="size-6"
							disabled={i === 0}
							onclick={() => moveDevice(device.id, "up", allDeviceIds)}
						>
							<ChevronUpIcon class="size-3.5" />
						</Button>
						<Button
							variant="ghost"
							size="icon"
							class="size-6"
							disabled={i === orderedDevices.length - 1}
							onclick={() => moveDevice(device.id, "down", allDeviceIds)}
						>
							<ChevronDownIcon class="size-3.5" />
						</Button>
					</div>
					<div class="flex size-9 items-center justify-center rounded-md bg-muted">
						<DeviceIcon class="size-4 text-muted-foreground" />
					</div>
					<div class="flex-1">
						<p class="text-sm font-medium">{device.name}</p>
						<p class="text-xs text-muted-foreground">
							{controlMethodLabels[device.controlMethod]}{device.ipAddress ? ` · ${device.ipAddress}` : ""}
						</p>
					</div>
					<Button
						variant="ghost"
						size="icon"
						onclick={() => toggleDeviceVisibility(device.id)}
						title={hidden ? "Show on dashboard" : "Hide from dashboard"}
					>
						{#if hidden}
							<EyeOffIcon class="size-4 text-muted-foreground" />
						{:else}
							<EyeIcon class="size-4 text-muted-foreground" />
						{/if}
					</Button>
					<Button variant="ghost" size="icon" onclick={() => (editingDeviceId = device.id)}>
						<PencilIcon class="size-4 text-muted-foreground" />
					</Button>
				</div>
			{/each}
		</div>
		<p class="text-xs text-muted-foreground">
			Use arrows to reorder devices on the dashboard. Click the eye icon to show or hide a device.
		</p>
	{/if}
</div>

{#if editingDevice}
	<EditDeviceModal
		device={editingDevice}
		open={!!editingDeviceId}
		onOpenChange={(open) => !open && (editingDeviceId = null)}
	/>
{/if}
