<script lang="ts">
	import PageHeader from "$lib/components/page-header.svelte";
	import AddSensorModal from "$lib/components/add-sensor-modal.svelte";
	import AddDeviceModal from "$lib/components/add-device-modal.svelte";
	import EditSensorModal from "$lib/components/edit-sensor-modal.svelte";
	import EditDeviceModal from "$lib/components/edit-device-modal.svelte";
	import SystemInfoCard from "$lib/components/system-info-card.svelte";
	import * as Select from "$lib/components/ui/select/index.js";
	import { Button } from "$lib/components/ui/button/index.js";
	import { Input } from "$lib/components/ui/input/index.js";
	import { Label } from "$lib/components/ui/label/index.js";
	import { Progress } from "$lib/components/ui/progress/index.js";
	import { sensors, ppfdCalibration, calibratePpfd, resetPpfdCalibration } from "$lib/stores/sensors.svelte";
	import { devices } from "$lib/stores/devices.svelte";
	import { settings, updateSettings, setTimezoneOffset, type Theme, type TemperatureUnit } from "$lib/stores/settings.svelte";
	import { systemInfo, initSystemInfoWebSocket } from "$lib/stores/system.svelte";
	import { websocket } from "$lib/stores/websocket.svelte";
	import { toast } from "svelte-sonner";
	import { sensorIcons, deviceIcons } from "$lib/icons";
	import PencilIcon from "@lucide/svelte/icons/pencil";
	import DownloadIcon from "@lucide/svelte/icons/download";
	import UploadIcon from "@lucide/svelte/icons/upload";
	import ThermometerIcon from "@lucide/svelte/icons/thermometer";
	import PowerIcon from "@lucide/svelte/icons/power";
	import RefreshCwIcon from "@lucide/svelte/icons/refresh-cw";
	import type { Sensor, Device } from "$lib/types";
	import { onMount } from "svelte";

	onMount(() => {
		initSystemInfoWebSocket();
	});

	const hardwareLabels: Record<Sensor["hardwareType"], string> = {
		sht3x: "SHT3x",
		sht4x: "SHT4x",
		scd4x: "SCD4x",
		as7341: "AS7341",
		calculated: "Calculated",
	};

	const controlMethodLabels: Record<Device["controlMethod"], string> = {
		shelly_gen1: "Shelly Gen1",
		shelly_gen2: "Shelly Gen2/Plus",
		tasmota: "Tasmota",
	};

	const themeOptions: { value: Theme; label: string }[] = [
		{ value: "system", label: "System" },
		{ value: "light", label: "Light" },
		{ value: "dark", label: "Dark" },
	];

	const temperatureUnitOptions: { value: TemperatureUnit; label: string }[] = [
		{ value: "celsius", label: "Celsius (°C)" },
		{ value: "fahrenheit", label: "Fahrenheit (°F)" },
	];

	const timezoneOptions: { value: number; label: string }[] = [
		{ value: -480, label: "UTC-8 (Los Angeles)" },
		{ value: -300, label: "UTC-5 (New York)" },
		{ value: 0, label: "UTC+0 (London)" },
		{ value: 60, label: "UTC+1 (Berlin)" },
		{ value: 120, label: "UTC+2 (Cairo)" },
		{ value: 180, label: "UTC+3 (Moscow)" },
		{ value: 330, label: "UTC+5:30 (Mumbai)" },
		{ value: 480, label: "UTC+8 (Singapore)" },
		{ value: 540, label: "UTC+9 (Tokyo)" },
		{ value: 600, label: "UTC+10 (Sydney)" },
	];

	let editingSensorId = $state<string | null>(null);
	let editingDeviceId = $state<string | null>(null);
	let ppfdInput = $state("");
	let backingUp = $state(false);
	let restoring = $state(false);
	let fileInput: HTMLInputElement | null = $state(null);
	let firmwareFileInput: HTMLInputElement | null = $state(null);
	let otaStatus = $state<"idle" | "uploading" | "downloading" | "installing" | "success" | "error" | "rebooting">("idle");
	let otaProgress = $state(0);
	let otaError = $state("");
	let checkingUpdate = $state(false);
	let updateAvailable = $state(false);
	let latestVersion = $state("");
	let downloadUrl = $state("");
	let firmwareFits = $state(true);
	let sizeWarning = $state("");
	let rateLimited = $state(false);
	let maxFirmwareSize = $state(0);
	let firmwareSize = $state(0);
	let reconnecting = $state(false);

	const editingSensor = $derived(sensors.find((s) => s.id === editingSensorId));
	const editingDevice = $derived(devices.find((d) => d.id === editingDeviceId));
	const hasAs7341 = $derived(sensors.some((s) => s.hardwareType === "as7341"));
	const isCalibrated = $derived(ppfdCalibration.factor !== 1.0);
	const otaInProgress = $derived(otaStatus !== "idle" && otaStatus !== "error");

	$effect(() => {
		const unsubscribe = websocket.on("ota_status", (data: any) => {
			otaStatus = data.status;
			if (data.progress !== undefined) {
				otaProgress = data.progress;
			}
			if (data.error) {
				otaError = data.error;
				toast.error(`OTA failed: ${data.error}`);
			}
			if (data.status === "success") {
				toast.success("Firmware update successful!");
				// Device will reboot shortly after successful flash
				reconnecting = true;
				setTimeout(() => {
					window.location.reload();
				}, 10000);
			}
			if (data.status === "rebooting") {
				reconnecting = true;
				setTimeout(() => {
					window.location.reload();
				}, 10000);
			}
		});

		return () => unsubscribe();
	});

	function handleCalibrate() {
		const value = parseFloat(ppfdInput);
		if (value > 0 && value <= 3000) {
			calibratePpfd(value);
			ppfdInput = "";
		}
	}

	async function handleBackupConfig() {
		backingUp = true;
		try {
			const response = await fetch("/api/config/backup");
			if (!response.ok) throw new Error(`Backup failed: ${response.status}`);
			const blob = await response.blob();
			const url = URL.createObjectURL(blob);
			const a = document.createElement("a");
			a.href = url;
			a.download = "espgrow-backup.json";
			a.click();
			URL.revokeObjectURL(url);
			toast.success("Configuration backed up successfully!");
		} catch (error) {
			console.error("Config backup failed:", error);
			toast.error(error instanceof Error ? error.message : "Backup failed");
		} finally {
			backingUp = false;
		}
	}

	function handleRestoreClick() {
		fileInput?.click();
	}

	async function handleFileSelect(event: Event) {
		const target = event.target as HTMLInputElement;
		const file = target.files?.[0];
		if (!file) return;

		restoring = true;
		try {
			const text = await file.text();
			const config = JSON.parse(text);

			if (!config.devices || !config.rules || !config.sensors) {
				throw new Error("Invalid backup file: missing required keys (devices, rules, sensors)");
			}

			if (!Array.isArray(config.devices) || !Array.isArray(config.rules) || !Array.isArray(config.sensors)) {
				throw new Error("Invalid backup file: devices, rules, and sensors must be arrays");
			}

			const response = await fetch("/api/config/restore", {
				method: "POST",
				headers: { "Content-Type": "application/json" },
				body: text
			});

			if (!response.ok) {
				const error = await response.json();
				throw new Error(error.error || `Restore failed: ${response.status}`);
			}

			toast.success("Configuration restored successfully!");
		} catch (error) {
			console.error("Config restore failed:", error);
			toast.error(error instanceof Error ? error.message : "Restore failed");
		} finally {
			restoring = false;
			if (target) target.value = "";
		}
	}

	function handleFirmwareUploadClick() {
		firmwareFileInput?.click();
	}

	async function handleFirmwareFileSelect(event: Event) {
		const target = event.target as HTMLInputElement;
		const file = target.files?.[0];
		if (!file) return;

		if (!file.name.endsWith(".bin")) {
			toast.error("Please select a .bin firmware file");
			target.value = "";
			return;
		}

		otaStatus = "uploading";
		otaProgress = 0;
		otaError = "";

		try {
			const formData = new FormData();
			formData.append("firmware", file);

			const xhr = new XMLHttpRequest();
			xhr.upload.onprogress = (e) => {
				if (e.lengthComputable) {
					otaProgress = Math.round((e.loaded / e.total) * 100);
				}
			};

			xhr.onload = () => {
				// Success response received (but device will reboot soon)
				// Don't set status here - WebSocket will handle it
			};

			xhr.onerror = () => {
				// Network error can occur if device reboots before response completes
				// Only treat as error if we're still in uploading state after a delay
				setTimeout(() => {
					if (otaStatus === "uploading") {
						otaStatus = "error";
						otaError = "Upload failed";
						toast.error("Upload failed: Network error");
					}
				}, 2000);
			};

			xhr.open("POST", "/api/ota/upload");
			xhr.send(formData);
		} catch (error) {
			otaStatus = "error";
			otaError = error instanceof Error ? error.message : "Upload failed";
			toast.error(`Upload failed: ${otaError}`);
		} finally {
			target.value = "";
		}
	}

	async function handleCheckUpdate() {
		checkingUpdate = true;
		try {
			const response = await fetch("/api/ota/check");
			if (!response.ok) throw new Error("Failed to check for updates");

			const data = await response.json();
			latestVersion = data.latestVersion || "";
			downloadUrl = data.downloadUrl || "";
			firmwareFits = data.fits !== false;
			sizeWarning = data.sizeWarning || "";
			rateLimited = data.rateLimited === true;
			maxFirmwareSize = data.maxFirmwareSize || 0;
			firmwareSize = data.size || 0;
			updateAvailable = data.currentVersion !== data.latestVersion && !!data.downloadUrl;

			if (rateLimited) {
				toast.info("GitHub rate limit reached. Try again later.");
			} else if (updateAvailable) {
				if (!firmwareFits) {
					toast.warning(`Update ${latestVersion} available but too large for this device`);
				} else {
					toast.success(`Update available: ${latestVersion}`);
				}
			} else if (!data.downloadUrl) {
				toast.info("No firmware found for your chip model");
			} else {
				toast.info("You're running the latest version");
			}
		} catch (error) {
			console.error("Update check failed:", error);
			toast.error(error instanceof Error ? error.message : "Update check failed");
		} finally {
			checkingUpdate = false;
		}
	}

	async function handleInstallUpdate() {
		if (!downloadUrl) return;

		otaStatus = "downloading";
		otaProgress = 0;
		otaError = "";

		try {
			const response = await fetch("/api/ota/install", {
				method: "POST",
				headers: { "Content-Type": "application/json" },
				body: JSON.stringify({ url: downloadUrl })
			});

			if (!response.ok) {
				const error = await response.json();
				throw new Error(error.error || "Install failed");
			}
		} catch (error) {
			otaStatus = "error";
			otaError = error instanceof Error ? error.message : "Install failed";
			toast.error(`Install failed: ${otaError}`);
		}
	}
</script>

<PageHeader title="Settings" />
<div class="flex flex-1 flex-col gap-6 p-4 pt-0">
	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Preferences</h2>
		<div class="divide-y divide-border rounded-lg border">
			<div class="flex items-center justify-between p-3">
				<Label>Temperature Unit</Label>
				<Select.Root
					type="single"
					value={settings.temperatureUnit}
					onValueChange={(v) => v && updateSettings({ temperatureUnit: v as TemperatureUnit })}
				>
					<Select.Trigger class="w-44">
						<span>{temperatureUnitOptions.find((o) => o.value === settings.temperatureUnit)?.label}</span>
					</Select.Trigger>
					<Select.Content>
						{#each temperatureUnitOptions as option (option.value)}
							<Select.Item value={option.value}>{option.label}</Select.Item>
						{/each}
					</Select.Content>
				</Select.Root>
			</div>
			<div class="flex items-center justify-between p-3">
				<Label>Theme</Label>
				<Select.Root
					type="single"
					value={settings.theme}
					onValueChange={(v) => v && updateSettings({ theme: v as Theme })}
				>
					<Select.Trigger class="w-44">
						<span>{themeOptions.find((o) => o.value === settings.theme)?.label}</span>
					</Select.Trigger>
					<Select.Content>
						{#each themeOptions as option (option.value)}
							<Select.Item value={option.value}>{option.label}</Select.Item>
						{/each}
					</Select.Content>
				</Select.Root>
			</div>
			<div class="flex items-center justify-between p-3">
				<Label>Timezone</Label>
				<Select.Root
					type="single"
					value={settings.timezoneOffsetMinutes.toString()}
					onValueChange={(v) => v && setTimezoneOffset(parseInt(v, 10))}
				>
					<Select.Trigger class="w-44">
						<span>{timezoneOptions.find((o) => o.value === settings.timezoneOffsetMinutes)?.label || `UTC${settings.timezoneOffsetMinutes >= 0 ? '+' : ''}${(settings.timezoneOffsetMinutes / 60).toFixed(settings.timezoneOffsetMinutes % 60 === 0 ? 0 : 1)}`}</span>
					</Select.Trigger>
					<Select.Content>
						{#each timezoneOptions as option (option.value)}
							<Select.Item value={option.value.toString()}>{option.label}</Select.Item>
						{/each}
					</Select.Content>
				</Select.Root>
			</div>
		</div>
	</section>

	{#if hasAs7341}
		<section>
			<h2 class="mb-3 text-sm font-medium text-muted-foreground">PPFD Calibration</h2>
			<div class="divide-y divide-border rounded-lg border">
				<div class="flex items-center justify-between p-3">
					<div>
						<p class="text-sm font-medium">Calibration Factor</p>
						<p class="text-xs text-muted-foreground">
							{#if isCalibrated}
								{ppfdCalibration.factor.toFixed(4)}
							{:else}
								Not calibrated (using defaults)
							{/if}
						</p>
					</div>
					{#if isCalibrated}
						<Button
							variant="outline"
							size="sm"
							disabled={ppfdCalibration.loading}
							onclick={resetPpfdCalibration}
						>
							Reset
						</Button>
					{/if}
				</div>
				<div class="space-y-3 p-3">
					<div>
						<p class="text-sm font-medium">Calibrate with known PPFD</p>
						<p class="mt-1 text-xs text-muted-foreground">
							Place the sensor where you know the exact PPFD from your grow light's map, then enter that value.
						</p>
					</div>
					<div class="flex gap-2">
						<Input
							type="number"
							placeholder="e.g. 400"
							min="1"
							max="3000"
							step="1"
							bind:value={ppfdInput}
							class="w-32"
						/>
						<Button
							size="sm"
							disabled={ppfdCalibration.loading || !ppfdInput || parseFloat(ppfdInput) <= 0}
							onclick={handleCalibrate}
						>
							{ppfdCalibration.loading ? "Calibrating..." : "Calibrate"}
						</Button>
					</div>
					{#if ppfdCalibration.error}
						<p class="text-xs text-destructive">{ppfdCalibration.error}</p>
					{/if}
				</div>
			</div>
		</section>
	{/if}

	<section>
		<div class="mb-3 flex items-center justify-between">
			<h2 class="text-sm font-medium text-muted-foreground">Sensors</h2>
			<AddSensorModal />
		</div>
		{#if sensors.length === 0}
			<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
				<ThermometerIcon class="size-8 text-muted-foreground/50" />
				<p class="mt-3 text-sm font-medium">No sensors</p>
				<p class="mt-1 text-xs text-muted-foreground">Add a sensor to start monitoring your grow tent</p>
			</div>
		{:else}
			<div class="divide-y divide-border rounded-lg border">
				{#each sensors as sensor (sensor.id)}
					{@const SensorIcon = sensorIcons[sensor.type]}
					<div class="flex items-center gap-3 p-3">
						<div class="flex size-9 items-center justify-center rounded-md bg-muted">
							<SensorIcon class="size-4 text-muted-foreground" />
						</div>
						<div class="flex-1">
							<p class="text-sm font-medium">{sensor.name}</p>
							<p class="text-xs text-muted-foreground">
								{hardwareLabels[sensor.hardwareType]}{sensor.address ? ` · ${sensor.address}` : ""}
							</p>
						</div>
						<Button variant="ghost" size="icon" onclick={() => (editingSensorId = sensor.id)}>
							<PencilIcon class="size-4 text-muted-foreground" />
						</Button>
					</div>
				{/each}
			</div>
		{/if}
	</section>

	<section>
		<div class="mb-3 flex items-center justify-between">
			<h2 class="text-sm font-medium text-muted-foreground">Devices</h2>
			<AddDeviceModal />
		</div>
		{#if devices.length === 0}
			<div class="flex flex-col items-center justify-center rounded-lg border border-dashed py-12 text-center">
				<PowerIcon class="size-8 text-muted-foreground/50" />
				<p class="mt-3 text-sm font-medium">No devices</p>
				<p class="mt-1 text-xs text-muted-foreground">Add a device to control fans, lights, or other equipment</p>
			</div>
		{:else}
			<div class="divide-y divide-border rounded-lg border">
				{#each devices as device (device.id)}
					{@const DeviceIcon = deviceIcons[device.type]}
					<div class="flex items-center gap-3 p-3">
						<div class="flex size-9 items-center justify-center rounded-md bg-muted">
							<DeviceIcon class="size-4 text-muted-foreground" />
						</div>
						<div class="flex-1">
							<p class="text-sm font-medium">{device.name}</p>
							<p class="text-xs text-muted-foreground">
								{controlMethodLabels[device.controlMethod]}{device.ipAddress ? ` · ${device.ipAddress}` : ""}
							</p>
						</div>
						<Button variant="ghost" size="icon" onclick={() => (editingDeviceId = device.id)}>
							<PencilIcon class="size-4 text-muted-foreground" />
						</Button>
					</div>
				{/each}
			</div>
		{/if}
	</section>

	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Backup & Restore</h2>
		<div class="divide-y divide-border rounded-lg border">
			<div class="flex items-center justify-between p-3">
				<div>
					<p class="text-sm font-medium">Backup Configuration</p>
					<p class="text-xs text-muted-foreground">Download sensors, devices, and rules as JSON</p>
				</div>
				<Button variant="outline" size="sm" disabled={backingUp} onclick={handleBackupConfig}>
					<DownloadIcon class="size-4" />
					{backingUp ? "Backing up..." : "Backup"}
				</Button>
			</div>
			<div class="flex items-center justify-between p-3">
				<div>
					<p class="text-sm font-medium">Restore Configuration</p>
					<p class="text-xs text-muted-foreground">Replace all settings from a backup file</p>
				</div>
				<Button variant="outline" size="sm" disabled={restoring} onclick={handleRestoreClick}>
					<UploadIcon class="size-4" />
					{restoring ? "Restoring..." : "Restore"}
				</Button>
			</div>
		</div>
		<input
			bind:this={fileInput}
			type="file"
			accept="application/json,.json"
			onchange={handleFileSelect}
			class="hidden"
		/>
	</section>

	<section>
		<h2 class="mb-3 text-sm font-medium text-muted-foreground">Firmware Update</h2>
		<div class="divide-y divide-border rounded-lg border">
			<div class="flex items-center justify-between p-3">
				<div class="flex-1">
					<p class="text-sm font-medium">Current Version</p>
					<p class="text-xs text-muted-foreground">{systemInfo.data?.firmwareVersion || "Unknown"}</p>
				</div>
			</div>
			
		<div class="flex items-center justify-between p-3">
			<div class="flex-1">
				<p class="text-sm font-medium">Check for Updates</p>
				<p class="text-xs text-muted-foreground">
					{#if rateLimited}
						GitHub rate limit reached — try again later
					{:else if updateAvailable}
						Update available: {latestVersion}
					{:else}
						Check GitHub for latest release
					{/if}
				</p>
			</div>
			<div class="flex gap-2">
				<Button 
					variant="outline" 
					size="sm" 
					disabled={checkingUpdate || otaInProgress}
					onclick={handleCheckUpdate}
				>
					<RefreshCwIcon class={checkingUpdate ? "size-4 animate-spin" : "size-4"} />
					Check
				</Button>
				{#if updateAvailable && !rateLimited}
					<Button 
						size="sm" 
						disabled={otaInProgress || !firmwareFits}
						onclick={handleInstallUpdate}
					>
						<DownloadIcon class="size-4" />
						Install
					</Button>
				{/if}
			</div>
		</div>
		
		{#if sizeWarning}
			<div class="border-t border-border p-3">
				<div class="rounded-md bg-destructive/10 p-3">
					<p class="text-xs font-medium text-destructive">{sizeWarning}</p>
					{#if maxFirmwareSize > 0 && firmwareSize > 0}
						<p class="mt-1 text-xs text-destructive/80">
							Firmware: {(firmwareSize / 1024).toFixed(0)} KB / Partition: {(maxFirmwareSize / 1024).toFixed(0)} KB
						</p>
					{/if}
				</div>
			</div>
		{/if}
			
			<div class="flex items-center justify-between p-3">
				<div class="flex-1">
					<p class="text-sm font-medium">Upload Firmware</p>
					<p class="text-xs text-muted-foreground">Manually upload a .bin file</p>
				</div>
				<Button 
					variant="outline" 
					size="sm" 
					disabled={otaInProgress}
					onclick={handleFirmwareUploadClick}
				>
					<UploadIcon class="size-4" />
					Upload
				</Button>
			</div>
			
			{#if otaInProgress || otaStatus === "error"}
				<div class="p-3">
					<div class="space-y-2">
						<div class="flex items-center justify-between">
							<p class="text-sm font-medium">
								{#if otaStatus === "uploading"}
									Uploading firmware...
								{:else if otaStatus === "downloading"}
									Downloading firmware...
								{:else if otaStatus === "installing"}
									Installing firmware...
								{:else if otaStatus === "success"}
									Update successful!
								{:else if otaStatus === "error"}
									Update failed
								{/if}
							</p>
							{#if otaInProgress}
								<span class="text-xs text-muted-foreground">{otaProgress}%</span>
							{/if}
						</div>
						<Progress value={otaProgress} class="h-2" />
						{#if otaStatus === "error" && otaError}
							<p class="text-xs text-destructive">{otaError}</p>
						{/if}
					</div>
				</div>
			{/if}
		</div>
		<input
			bind:this={firmwareFileInput}
			type="file"
			accept=".bin"
			onchange={handleFirmwareFileSelect}
			class="hidden"
		/>
	</section>

	<SystemInfoCard />
</div>

{#if reconnecting}
	<div class="fixed inset-0 z-50 flex items-center justify-center bg-background/80 backdrop-blur-sm">
		<div class="flex flex-col items-center gap-4 rounded-lg border bg-card p-8 shadow-lg">
			<div class="size-12 animate-spin rounded-full border-4 border-muted border-t-primary"></div>
			<div class="text-center">
				<p class="text-lg font-semibold">Rebooting...</p>
				<p class="text-sm text-muted-foreground">Reconnecting in a moment</p>
			</div>
		</div>
	</div>
{/if}

<div class="h-20"></div>

{#if editingSensor}
	<EditSensorModal
		sensor={editingSensor}
		open={!!editingSensorId}
		onOpenChange={(open) => !open && (editingSensorId = null)}
	/>
{/if}

{#if editingDevice}
	<EditDeviceModal
		device={editingDevice}
		open={!!editingDeviceId}
		onOpenChange={(open) => !open && (editingDeviceId = null)}
	/>
{/if}
