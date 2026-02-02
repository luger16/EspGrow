<script lang="ts">
	import "./layout.css";
	import favicon from "$lib/assets/favicon.svg";
	import AppSidebar from "$lib/components/app-sidebar.svelte";
	import BottomTabs from "$lib/components/bottom-tabs.svelte";
	import * as Sidebar from "$lib/components/ui/sidebar/index.js";
	import { initTheme } from "$lib/stores/settings.svelte";
	import { websocket } from "$lib/stores/websocket.svelte";
	import { initSensorWebSocket } from "$lib/stores/sensors.svelte";
	import { onMount } from "svelte";

	let { children } = $props();

	onMount(() => {
		initTheme();
		websocket.connect();
		initSensorWebSocket();
		return () => websocket.disconnect();
	});
</script>

<svelte:head>
	<link rel="icon" href={favicon} />
</svelte:head>

<Sidebar.Provider>
	<AppSidebar />
	<Sidebar.Inset class="pb-20 md:pb-0">
		{@render children()}
	</Sidebar.Inset>
	<BottomTabs />
</Sidebar.Provider>
