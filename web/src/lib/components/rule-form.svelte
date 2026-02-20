<script lang="ts">
	import * as Select from "$lib/components/ui/select/index.js";
	import * as Tabs from "$lib/components/ui/tabs/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { Switch } from "$lib/components/ui/switch/index.js";
	import { sensors } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import { formatUnit } from "$lib/utils";
	import type { AutomationRule, ComparisonOperator } from "$lib/types";

	type Props = {
		ruleType: "sensor" | "schedule";
		name: string;
		sensorId: string;
		operator: ComparisonOperator;
		threshold: string;
		useHysteresis: boolean;
		thresholdOff: string;
		onTime: string;
		offTime: string;
		minRunTimeMin: string;
		deviceId: string;
		action: AutomationRule["action"];
		submitted: boolean;
	};

	let {
		ruleType = $bindable(),
		name = $bindable(),
		sensorId = $bindable(),
		operator = $bindable(),
		threshold = $bindable(),
		useHysteresis = $bindable(),
		thresholdOff = $bindable(),
		onTime = $bindable(),
		offTime = $bindable(),
		minRunTimeMin = $bindable(),
		deviceId = $bindable(),
		action = $bindable(),
		submitted,
	}: Props = $props();

	const operatorOptions: { value: ComparisonOperator; label: string }[] = [
		{ value: ">", label: ">" },
		{ value: ">=", label: ">=" },
		{ value: "<", label: "<" },
		{ value: "<=", label: "<=" },
		{ value: "=", label: "=" },
	];

	const actionOptions: { value: AutomationRule["action"]; label: string }[] = [
		{ value: "turn_on", label: "Turn On" },
		{ value: "turn_off", label: "Turn Off" },
	];

	const selectedSensor = $derived(sensors.find((s) => s.id === sensorId));
</script>

<Tabs.Root bind:value={ruleType}>
	<Tabs.List class="grid w-full grid-cols-2">
		<Tabs.Trigger value="sensor">Sensor Rule</Tabs.Trigger>
		<Tabs.Trigger value="schedule">Schedule Rule</Tabs.Trigger>
	</Tabs.List>
</Tabs.Root>

<div class="grid gap-4">
	<div class="grid gap-2">
		<Label for="name">Rule Name</Label>
		<Input id="name" bind:value={name} placeholder="e.g. High Temperature Fan" required />
		{#if submitted && !name}
			<p class="text-destructive text-xs">Rule name is required</p>
		{/if}
	</div>

	{#if ruleType === "sensor"}
		<div class="grid gap-2">
			<Label>When</Label>
			<Select.Root type="single" value={sensorId} onValueChange={(v) => v && (sensorId = v)}>
				<Select.Trigger>
					<span>{selectedSensor?.name ?? "Select sensor..."}</span>
				</Select.Trigger>
				<Select.Content>
					{#each sensors as sensor (sensor.id)}
						<Select.Item value={sensor.id}>{sensor.name}</Select.Item>
					{/each}
				</Select.Content>
			</Select.Root>
			{#if submitted && !sensorId}
				<p class="text-destructive text-xs">Select a sensor</p>
			{/if}
		</div>

		<div class="grid grid-cols-[80px_1fr] gap-2">
			<Select.Root type="single" value={operator} onValueChange={(v) => v && (operator = v as ComparisonOperator)}>
				<Select.Trigger>
					<span>{operator}</span>
				</Select.Trigger>
				<Select.Content>
					{#each operatorOptions as opt (opt.value)}
						<Select.Item value={opt.value}>{opt.label}</Select.Item>
					{/each}
				</Select.Content>
			</Select.Root>
			<Input
				type="number"
				bind:value={threshold}
				placeholder={selectedSensor ? `Value (${selectedSensor.unit})` : "Value"}
				required
			/>
		</div>
		{#if submitted && !threshold}
			<p class="text-destructive text-xs -mt-2">Threshold value is required</p>
		{/if}

		<div class="grid gap-3 rounded-lg border p-3">
			<div class="flex items-center justify-between">
				<div class="space-y-0.5">
					<Label class="text-sm font-medium">Enable Hysteresis</Label>
					<p class="text-xs text-muted-foreground">Use two thresholds to prevent rapid switching</p>
				</div>
				<Switch bind:checked={useHysteresis} />
			</div>

			{#if useHysteresis}
				<div class="grid gap-2">
					<Label for="thresholdOff">Turn Off Threshold</Label>
					<Input
						id="thresholdOff"
						type="number"
						bind:value={thresholdOff}
						placeholder={selectedSensor ? `Value (${selectedSensor.unit})` : "Value"}
						required={useHysteresis}
					/>
					{#if submitted && useHysteresis && !thresholdOff}
						<p class="text-destructive text-xs">Turn off threshold is required</p>
					{/if}
					<p class="text-xs text-muted-foreground">
						Device turns on at {threshold || "___"}{formatUnit(selectedSensor?.unit || "")}, turns off at {thresholdOff || "___"}{formatUnit(selectedSensor?.unit || "")}
					</p>
				</div>
			{/if}
		</div>

		<div class="grid gap-2">
			<Label for="minRunTime">Minimum Run Time (minutes)</Label>
			<Input
				id="minRunTime"
				type="number"
				bind:value={minRunTimeMin}
				placeholder="Optional (e.g. 2)"
			/>
			<p class="text-xs text-muted-foreground">Device must stay in new state for at least this long</p>
		</div>
	{:else}
		<div class="grid gap-2">
			<Label for="onTime">Turn On Time</Label>
			<Input
				id="onTime"
				type="time"
				bind:value={onTime}
				required={ruleType === "schedule"}
			/>
			{#if submitted && !onTime}
				<p class="text-destructive text-xs">On time is required</p>
			{/if}
		</div>

		<div class="grid gap-2">
			<Label for="offTime">Turn Off Time</Label>
			<Input
				id="offTime"
				type="time"
				bind:value={offTime}
				required={ruleType === "schedule"}
			/>
			{#if submitted && !offTime}
				<p class="text-destructive text-xs">Off time is required</p>
			{/if}
		</div>
	{/if}

	<div class="grid gap-2">
		<Label>Then</Label>
		<div class="grid grid-cols-[1fr_120px] gap-2">
			<Select.Root type="single" value={deviceId} onValueChange={(v) => v && (deviceId = v)}>
				<Select.Trigger>
					<span>{devices.find((d) => d.id === deviceId)?.name ?? "Select device..."}</span>
				</Select.Trigger>
				<Select.Content>
					{#each devices as device (device.id)}
						<Select.Item value={device.id}>{device.name}</Select.Item>
					{/each}
				</Select.Content>
			</Select.Root>
			<Select.Root type="single" value={action} onValueChange={(v) => v && (action = v as AutomationRule["action"])}>
				<Select.Trigger>
					<span>{actionOptions.find((a) => a.value === action)?.label}</span>
				</Select.Trigger>
				<Select.Content>
					{#each actionOptions as opt (opt.value)}
						<Select.Item value={opt.value}>{opt.label}</Select.Item>
					{/each}
				</Select.Content>
			</Select.Root>
		</div>
		{#if submitted && !deviceId}
			<p class="text-destructive text-xs">Select a device</p>
		{/if}
	</div>
</div>
