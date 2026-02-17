<script lang="ts">
	import * as Sidebar from "$lib/components/ui/sidebar/index.js";
	import { page } from "$app/state";
	import Logo from "$lib/assets/logo.svg?raw";
	import LayoutDashboardIcon from "@lucide/svelte/icons/layout-dashboard";
	import ZapIcon from "@lucide/svelte/icons/zap";
	import SettingsIcon from "@lucide/svelte/icons/settings";
	import type { ComponentProps } from "svelte";

	let { ref = $bindable(null), ...restProps }: ComponentProps<typeof Sidebar.Root> = $props();

	const navItems = [
		{ title: "Dashboard", url: "/", icon: LayoutDashboardIcon },
		{ title: "Automation", url: "/automation", icon: ZapIcon },
		{ title: "Settings", url: "/settings", icon: SettingsIcon },
	];
</script>

<Sidebar.Root bind:ref variant="inset" collapsible="icon" {...restProps}>
	<Sidebar.Header>
		<Sidebar.Menu>
			<Sidebar.MenuItem>
				<Sidebar.MenuButton size="lg">
					{#snippet child({ props })}
						<a href="/" {...props}>
							<div
								class="bg-sidebar-primary dark:bg-sidebar-accent text-sidebar-primary-foreground flex aspect-square size-8 items-center justify-center rounded-lg"
							>
								<div class="size-4 [&>svg]:size-full">{@html Logo}</div>
							</div>
							<span class="truncate font-semibold">EspGrow</span>
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
</Sidebar.Root>
