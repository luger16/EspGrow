<script lang="ts">
	import * as Dialog from "$lib/components/ui/dialog/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { ppfdCalibration, calibratePpfd, resetPpfdCalibration } from "$lib/stores/sensors.svelte";

	let open = $state(false);
	let ppfdInput = $state("");

	const isCalibrated = $derived(ppfdCalibration.factor !== 1.0);
	const canCalibrate = $derived(ppfdInput && parseFloat(ppfdInput) > 0 && parseFloat(ppfdInput) <= 3000);

	function handleCalibrate() {
		const value = parseFloat(ppfdInput);
		if (value > 0 && value <= 3000) {
			calibratePpfd(value);
			ppfdInput = "";
			open = false;
		}
	}

	function handleReset() {
		resetPpfdCalibration();
		open = false;
	}

	function handleOpenChange(isOpen: boolean) {
		open = isOpen;
		if (!isOpen) {
			ppfdInput = "";
		}
	}
</script>

<Dialog.Root {open} onOpenChange={handleOpenChange}>
	<Dialog.Trigger>
		{#snippet child({ props })}
			<Button {...props} variant="outline" size="sm">
				Calibrate
			</Button>
		{/snippet}
	</Dialog.Trigger>
	<Dialog.Content class="sm:max-w-md">
		<Dialog.Header>
			<Dialog.Title>PPFD Calibration</Dialog.Title>
			<Dialog.Description>
				Calibrate your light sensor using a known PPFD value from your grow light's map.
			</Dialog.Description>
		</Dialog.Header>
		<div class="grid gap-4 py-4">
			{#if isCalibrated}
				<div class="flex items-center justify-between rounded-lg border p-3">
					<div>
						<p class="text-sm font-medium">Current Factor</p>
						<p class="text-xs text-muted-foreground">{ppfdCalibration.factor.toFixed(4)}</p>
					</div>
					<Button
						variant="outline"
						size="sm"
						disabled={ppfdCalibration.loading}
						onclick={handleReset}
					>
						Reset
					</Button>
				</div>
			{/if}

			<div class="grid gap-2">
				<Label for="ppfd">Known PPFD Value</Label>
				<p class="text-xs text-muted-foreground">
					Place the sensor where you know the exact PPFD from your grow light's map, then enter that value.
				</p>
				<Input
					id="ppfd"
					type="number"
					placeholder="e.g. 400"
					min="1"
					max="3000"
					step="1"
					bind:value={ppfdInput}
				/>
				{#if ppfdCalibration.error}
					<p class="text-xs text-destructive">{ppfdCalibration.error}</p>
				{/if}
			</div>
		</div>
		<Dialog.Footer>
			<Button
				disabled={ppfdCalibration.loading || !canCalibrate}
				onclick={handleCalibrate}
			>
				{ppfdCalibration.loading ? "Calibrating..." : "Calibrate"}
			</Button>
		</Dialog.Footer>
	</Dialog.Content>
</Dialog.Root>
