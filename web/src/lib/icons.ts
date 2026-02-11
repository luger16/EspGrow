import ThermometerIcon from "@lucide/svelte/icons/thermometer";
import DropletIcon from "@lucide/svelte/icons/droplet";
import CloudIcon from "@lucide/svelte/icons/cloud";
import SunIcon from "@lucide/svelte/icons/sun";
import GaugeIcon from "@lucide/svelte/icons/gauge";
import FanIcon from "@lucide/svelte/icons/fan";
import LightbulbIcon from "@lucide/svelte/icons/lightbulb";
import HeaterIcon from "@lucide/svelte/icons/heater";
import WavesIcon from "@lucide/svelte/icons/waves";
import BubblesIcon from "@lucide/svelte/icons/droplets";
import DropletOffIcon from "@lucide/svelte/icons/droplet-off";
import type { Component } from "svelte";
import type { SensorType, DeviceType } from "$lib/types";

export const sensorIcons: Record<SensorType, Component> = {
	temperature: ThermometerIcon,
	humidity: DropletIcon,
	co2: CloudIcon,
	light: SunIcon,
	vpd: GaugeIcon,
};

export const deviceIcons: Record<DeviceType, Component> = {
	fan: FanIcon,
	light: LightbulbIcon,
	heater: HeaterIcon,
	pump: WavesIcon,
	humidifier: BubblesIcon,
	dehumidifier: DropletOffIcon,
};
