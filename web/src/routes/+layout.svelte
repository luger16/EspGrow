<script lang="ts">
	import "./layout.css";
	import favicon from "$lib/assets/favicon.svg";
	import AppSidebar from "$lib/components/app-sidebar.svelte";
	import BottomTabs from "$lib/components/bottom-tabs.svelte";
	import * as Sidebar from "$lib/components/ui/sidebar/index.js";

	let { children } = $props();
</script>

<svelte:head>
	<link rel="icon" href={favicon} />
</svelte:head>

<Sidebar.Provider>
	<div class="hidden md:contents">
		<AppSidebar />
	</div>
	<Sidebar.Inset class="pb-20 md:pb-0">
		{@render children()}
	</Sidebar.Inset>
	<BottomTabs />
</Sidebar.Provider>
