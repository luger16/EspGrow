<script lang="ts">
	import * as Sidebar from "$lib/components/ui/sidebar/index.js";

	let { title }: { title: string } = $props();
</script>

<header class="sticky top-0 z-40 flex h-14 shrink-0 items-center gap-2 bg-background px-4 md:relative">
	<Sidebar.Trigger class="-ms-1 hidden md:flex" />
	<h1 class="text-lg font-semibold">{title}</h1>
</header>
