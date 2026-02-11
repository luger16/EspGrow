<script lang="ts">
	import * as Sidebar from "$lib/components/ui/sidebar/index.js";
	import { websocket } from "$lib/stores/websocket.svelte";

	let { title }: { title: string } = $props();
</script>

<header class="sticky top-0 z-40 flex h-14 shrink-0 items-center gap-2 rounded-t-xl bg-background px-4 md:relative">
	<Sidebar.Trigger class="-ms-1 hidden md:flex" />
	<h1 class="text-lg font-semibold">{title}</h1>
	<div class="ml-auto flex items-center gap-2">
		<div
			class="size-2 rounded-full {websocket.connected ? 'bg-green-500' : 'bg-amber-500 animate-pulse'}"
			title={websocket.connected ? "Connected" : "Reconnecting…"}
		></div>
	</div>
</header>
