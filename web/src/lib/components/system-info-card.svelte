<script lang="ts">
	import * as Card from "$lib/components/ui/card/index.js";
	import { systemInfo, requestSystemInfo } from "$lib/stores/system.svelte";
	import { onMount } from "svelte";
	import ClockIcon from "@lucide/svelte/icons/clock";
	import MemoryStickIcon from "@lucide/svelte/icons/memory-stick";
	import WifiIcon from "@lucide/svelte/icons/wifi";
	import NetworkIcon from "@lucide/svelte/icons/network";
	import CpuIcon from "@lucide/svelte/icons/cpu";
	import PackageIcon from "@lucide/svelte/icons/package";

	function formatUptime(seconds: number): string {
		const days = Math.floor(seconds / 86400);
		const hours = Math.floor((seconds % 86400) / 3600);
		const mins = Math.floor((seconds % 3600) / 60);
		
		if (days > 0) return `${days}d ${hours}h`;
		if (hours > 0) return `${hours}h ${mins}m`;
		return `${mins}m`;
	}

	function formatBytes(bytes: number): string {
		if (bytes < 1024) return `${bytes} B`;
		if (bytes < 1048576) return `${(bytes / 1024).toFixed(1)} KB`;
		return `${(bytes / 1048576).toFixed(1)} MB`;
	}

	onMount(() => {
		requestSystemInfo();
		const interval = setInterval(requestSystemInfo, 30000);
		return () => clearInterval(interval);
	});
</script>

{#if systemInfo.data}
	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">System</h2>
		<div class="grid grid-cols-2 gap-3 md:grid-cols-3 lg:grid-cols-6">
			<Card.Root class="py-4">
				<Card.Content class="flex flex-col gap-3 px-4">
					<div class="flex items-center justify-between gap-2">
						<span class="text-xs font-medium text-muted-foreground">Uptime</span>
						<div class="flex size-8 shrink-0 items-center justify-center rounded-lg bg-muted">
							<ClockIcon class="size-4 text-muted-foreground" />
						</div>
					</div>
					<span class="text-lg font-semibold tabular-nums">{formatUptime(systemInfo.data.uptime)}</span>
				</Card.Content>
			</Card.Root>

			<Card.Root class="py-4">
				<Card.Content class="flex flex-col gap-3 px-4">
					<div class="flex items-center justify-between gap-2">
						<span class="text-xs font-medium text-muted-foreground">Memory</span>
						<div class="flex size-8 shrink-0 items-center justify-center rounded-lg bg-muted">
							<MemoryStickIcon class="size-4 text-muted-foreground" />
						</div>
					</div>
					<span class="text-lg font-semibold tabular-nums">{formatBytes(systemInfo.data.freeHeap)}</span>
				</Card.Content>
			</Card.Root>

			<Card.Root class="py-4">
				<Card.Content class="flex flex-col gap-3 px-4">
					<div class="flex items-center justify-between gap-2">
						<span class="text-xs font-medium text-muted-foreground">WiFi</span>
						<div class="flex size-8 shrink-0 items-center justify-center rounded-lg bg-muted">
							<WifiIcon class="size-4 text-muted-foreground" />
						</div>
					</div>
					<span class="text-lg font-semibold tabular-nums">{systemInfo.data.wifiRssi} dBm</span>
				</Card.Content>
			</Card.Root>

			<Card.Root class="py-4">
				<Card.Content class="flex flex-col gap-3 px-4">
					<div class="flex items-center justify-between gap-2">
						<span class="text-xs font-medium text-muted-foreground">IP</span>
						<div class="flex size-8 shrink-0 items-center justify-center rounded-lg bg-muted">
							<NetworkIcon class="size-4 text-muted-foreground" />
						</div>
					</div>
					<span class="text-sm font-semibold">{systemInfo.data.ipAddress}</span>
				</Card.Content>
			</Card.Root>

			<Card.Root class="py-4">
				<Card.Content class="flex flex-col gap-3 px-4">
					<div class="flex items-center justify-between gap-2">
						<span class="text-xs font-medium text-muted-foreground">Chip</span>
						<div class="flex size-8 shrink-0 items-center justify-center rounded-lg bg-muted">
							<CpuIcon class="size-4 text-muted-foreground" />
						</div>
					</div>
					<span class="text-sm font-semibold">{systemInfo.data.chipModel}</span>
				</Card.Content>
			</Card.Root>

			<Card.Root class="py-4">
				<Card.Content class="flex flex-col gap-3 px-4">
					<div class="flex items-center justify-between gap-2">
						<span class="text-xs font-medium text-muted-foreground">Version</span>
						<div class="flex size-8 shrink-0 items-center justify-center rounded-lg bg-muted">
							<PackageIcon class="size-4 text-muted-foreground" />
						</div>
					</div>
					<span class="text-sm font-semibold">v{systemInfo.data.firmwareVersion}</span>
				</Card.Content>
			</Card.Root>
		</div>
	</section>
{/if}
