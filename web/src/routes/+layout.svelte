<script lang="ts">
	import "./layout.css";
	import favicon from "$lib/assets/favicon.svg";
	import AppSidebar from "$lib/components/app-sidebar.svelte";
	import BottomTabs from "$lib/components/bottom-tabs.svelte";
	import { Toaster } from "$lib/components/ui/sonner";
	import * as Sidebar from "$lib/components/ui/sidebar/index.js";
	import { initTheme } from "$lib/stores/settings.svelte";
	import { websocket } from "$lib/stores/websocket.svelte";
	import { initSensorWebSocket, sensorHistory } from "$lib/stores/sensors.svelte";
	import { initDeviceWebSocket } from "$lib/stores/devices.svelte";
	import { initDeviceModesWebSocket } from "$lib/stores/device-modes.svelte";
	import { initClimateWebSocket } from "$lib/stores/climate.svelte";
	import { initEnergyWebSocket } from "$lib/stores/energy.svelte";
	import { onMount } from "svelte";

	let { children } = $props();

	onMount(() => {
		initTheme();
		websocket.connect();
		initSensorWebSocket();
		initDeviceWebSocket();
		initDeviceModesWebSocket();
		initClimateWebSocket();
		initEnergyWebSocket();
		websocket.send("get_init");
		websocket.send("get_events");
		return () => websocket.disconnect();
	});

	$effect(() => {
		if (websocket.connectCount > 1) {
			for (const key of Object.keys(sensorHistory)) {
				delete sensorHistory[key];
			}
			websocket.send("get_init");
			websocket.send("get_events");
		}
	});
</script>

<svelte:head>
	<title>EspGrow</title>
	<link rel="icon" href={favicon} />
</svelte:head>

<Sidebar.Provider>
	<AppSidebar />
	<Sidebar.Inset class="pb-20 md:pb-0">
		{@render children()}
	</Sidebar.Inset>
	<BottomTabs />
	<Toaster position="top-center" closeButton />
</Sidebar.Provider>
