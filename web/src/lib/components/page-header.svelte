<script lang="ts">
	import * as Sidebar from "$lib/components/ui/sidebar/index.js";

	let { title }: { title: string } = $props();
</script>

<header class="flex h-14 shrink-0 items-center gap-2 px-4">
	<Sidebar.Trigger class="-ms-1" />
	<h1 class="text-lg font-semibold">{title}</h1>
</header>
