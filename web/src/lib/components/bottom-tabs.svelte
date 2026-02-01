<script lang="ts">
	import { page } from "$app/state";
	import LayoutDashboardIcon from "@lucide/svelte/icons/layout-dashboard";
	import ZapIcon from "@lucide/svelte/icons/zap";
	import SettingsIcon from "@lucide/svelte/icons/settings";

	const navItems = [
		{ title: "Dashboard", url: "/", icon: LayoutDashboardIcon },
		{ title: "Automation", url: "/automation", icon: ZapIcon },
		{ title: "Settings", url: "/settings", icon: SettingsIcon },
	];
</script>

<nav class="fixed bottom-0 left-0 right-0 z-50 border-t border-border bg-background md:hidden" style="padding-bottom: max(0.5rem, env(safe-area-inset-bottom))">
	<div class="flex h-16 items-center justify-around">
		{#each navItems as item (item.title)}
			{@const isActive = page.url.pathname === item.url}
			<a
				href={item.url}
				class="flex flex-1 flex-col items-center justify-center gap-1 py-2 transition-colors {isActive
					? 'text-primary'
					: 'text-muted-foreground hover:text-foreground'}"
			>
				<item.icon class="size-5" />
				<span class="text-xs font-medium">{item.title}</span>
			</a>
		{/each}
	</div>
</nav>
