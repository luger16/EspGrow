<script lang="ts">
	import PageHeader from "$lib/components/page-header.svelte";
	import AddRuleModal from "$lib/components/add-rule-modal.svelte";
	import EditRuleModal from "$lib/components/edit-rule-modal.svelte";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Switch } from "$lib/components/ui/switch/index.js";
	import { rules, toggleRule } from "$lib/stores/rules.svelte";
	import { sensors } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import PencilIcon from "@lucide/svelte/icons/pencil";
	import ZapIcon from "@lucide/svelte/icons/zap";

	let editingRuleId = $state<string | null>(null);
	const editingRule = $derived(rules.find((r) => r.id === editingRuleId));

	function getSensorName(sensorId: string): string {
		return sensors.find((s) => s.id === sensorId)?.name ?? "Unknown sensor";
	}

	function getDeviceName(deviceId: string): string {
		return devices.find((d) => d.id === deviceId)?.name ?? "Unknown device";
	}

	function getSensorUnit(sensorId: string): string {
		return sensors.find((s) => s.id === sensorId)?.unit ?? "";
	}

	function formatAction(action: "turn_on" | "turn_off"): string {
		return action === "turn_on" ? "turn on" : "turn off";
	}
</script>

<PageHeader title="Automation" />
<div class="flex flex-1 flex-col gap-6 p-4 pt-0">
	<section>
		<div class="mb-3 flex items-center justify-between">
			<h2 class="text-sm font-medium text-muted-foreground">Rules</h2>
			<AddRuleModal />
		</div>
		{#if rules.length === 0}
			<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
				<ZapIcon class="size-8 text-muted-foreground/50" />
				<p class="mt-3 text-sm font-medium">No automation rules</p>
				<p class="mt-1 text-xs text-muted-foreground">Create rules to automate your devices based on sensor readings</p>
			</div>
		{:else}
			<div class="divide-y divide-border rounded-lg border">
				{#each rules as rule (rule.id)}
					<div class="flex items-center gap-3 p-3">
						<div class="flex size-9 items-center justify-center rounded-md bg-muted">
							<ZapIcon class="size-4 text-muted-foreground" />
						</div>
						<div class="flex-1">
							<p class="text-sm font-medium">{rule.name}</p>
							<p class="text-xs text-muted-foreground">
								If {getSensorName(rule.sensorId)} {rule.operator} {rule.threshold}{getSensorUnit(rule.sensorId)}, {formatAction(rule.action)} {getDeviceName(rule.deviceId)}
							</p>
						</div>
						<Switch checked={rule.enabled} onCheckedChange={() => toggleRule(rule.id)} />
						<Button variant="ghost" size="icon" onclick={() => (editingRuleId = rule.id)}>
							<PencilIcon class="size-4 text-muted-foreground" />
						</Button>
					</div>
				{/each}
			</div>
		{/if}
	</section>
</div>

{#if editingRule}
	<EditRuleModal
		rule={editingRule}
		open={!!editingRuleId}
		onOpenChange={(open) => !open && (editingRuleId = null)}
	/>
{/if}
