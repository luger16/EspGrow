<script lang="ts">
	import * as Sidebar from "$lib/components/ui/sidebar/index.js";
	import { page } from "$app/state";
	import { settings, updateSettings, initTheme } from "$lib/stores/settings.svelte";
	import SproutIcon from "@lucide/svelte/icons/sprout";
	import LayoutDashboardIcon from "@lucide/svelte/icons/layout-dashboard";
	import ZapIcon from "@lucide/svelte/icons/zap";
	import SettingsIcon from "@lucide/svelte/icons/settings";
	import SunIcon from "@lucide/svelte/icons/sun";
	import MoonIcon from "@lucide/svelte/icons/moon";
	import type { ComponentProps } from "svelte";

	let { ref = $bindable(null), ...restProps }: ComponentProps<typeof Sidebar.Root> = $props();

	const navItems = [
		{ title: "Dashboard", url: "/", icon: LayoutDashboardIcon },
		{ title: "Automation", url: "/automation", icon: ZapIcon },
		{ title: "Settings", url: "/settings", icon: SettingsIcon },
	];

	const isDark = $derived(
		settings.theme === "dark" ||
			(settings.theme === "system" && typeof window !== "undefined" && window.matchMedia("(prefers-color-scheme: dark)").matches)
	);

	$effect(() => {
		initTheme();
	});

	function toggleDarkMode() {
		updateSettings({ theme: isDark ? "light" : "dark" });
	}
</script>

<Sidebar.Root bind:ref variant="inset" collapsible="icon" {...restProps}>
	<Sidebar.Header>
		<Sidebar.Menu>
			<Sidebar.MenuItem>
				<Sidebar.MenuButton size="lg">
					{#snippet child({ props })}
						<a href="/" {...props}>
							<div
								class="bg-sidebar-primary text-sidebar-primary-foreground flex aspect-square size-8 items-center justify-center rounded-lg"
							>
								<SproutIcon class="size-4" />
							</div>
							<div class="grid flex-1 text-start text-sm leading-tight">
								<span class="truncate font-semibold">EspGrow</span>
								<span class="text-muted-foreground truncate text-xs">Grow Tent Monitor</span>
							</div>
						</a>
					{/snippet}
				</Sidebar.MenuButton>
			</Sidebar.MenuItem>
		</Sidebar.Menu>
	</Sidebar.Header>
	<Sidebar.Content>
		<Sidebar.Group>
			<Sidebar.GroupContent>
				<Sidebar.Menu>
					{#each navItems as item (item.title)}
						<Sidebar.MenuItem>
							<Sidebar.MenuButton tooltipContent={item.title} isActive={page.url.pathname === item.url}>
								{#snippet child({ props })}
									<a href={item.url} {...props}>
										<item.icon />
										<span>{item.title}</span>
									</a>
								{/snippet}
							</Sidebar.MenuButton>
						</Sidebar.MenuItem>
					{/each}
				</Sidebar.Menu>
			</Sidebar.GroupContent>
		</Sidebar.Group>
	</Sidebar.Content>
	<Sidebar.Footer>
		<Sidebar.Menu>
			<Sidebar.MenuItem>
				<Sidebar.MenuButton tooltipContent={isDark ? "Light mode" : "Dark mode"} onclick={toggleDarkMode}>
					{#if isDark}
						<SunIcon />
						<span>Light mode</span>
					{:else}
						<MoonIcon />
						<span>Dark mode</span>
					{/if}
				</Sidebar.MenuButton>
			</Sidebar.MenuItem>
		</Sidebar.Menu>
	</Sidebar.Footer>
</Sidebar.Root>
