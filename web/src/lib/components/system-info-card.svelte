<script lang="ts">
	import { systemInfo, requestSystemInfo } from "$lib/stores/system.svelte";
	import { onMount } from "svelte";

	function formatUptime(seconds: number): string {
		const days = Math.floor(seconds / 86400);
		const hours = Math.floor((seconds % 86400) / 3600);
		const mins = Math.floor((seconds % 3600) / 60);
		
		if (days > 0) return `${days}d ${hours}h`;
		if (hours > 0) return `${hours}h ${mins}m`;
		return `${mins}m`;
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
		<div class="grid grid-cols-2 gap-x-6 gap-y-2 rounded-lg border p-3 text-sm">
			<div class="flex justify-between">
				<span class="text-muted-foreground">Uptime</span>
				<span class="font-medium tabular-nums">{formatUptime(systemInfo.data.uptime)}</span>
			</div>
			<div class="flex justify-between">
				<span class="text-muted-foreground">WiFi</span>
				<span class="font-medium tabular-nums">{systemInfo.data.wifiRssi} dBm</span>
			</div>
			<div class="flex justify-between">
				<span class="text-muted-foreground">IP</span>
				<span class="font-medium">{systemInfo.data.ipAddress}</span>
			</div>
			<div class="flex justify-between">
				<span class="text-muted-foreground">Chip</span>
				<span class="font-medium">{systemInfo.data.chipModel}</span>
			</div>
		</div>
	</section>
{/if}
