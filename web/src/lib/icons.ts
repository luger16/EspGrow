import ThermometerIcon from "@lucide/svelte/icons/thermometer";
import DropletIcon from "@lucide/svelte/icons/droplet";
import CloudIcon from "@lucide/svelte/icons/cloud";
import SunIcon from "@lucide/svelte/icons/sun";
import LeafIcon from "@lucide/svelte/icons/leaf";
import GaugeIcon from "@lucide/svelte/icons/gauge";
import FanIcon from "@lucide/svelte/icons/fan";
import LightbulbIcon from "@lucide/svelte/icons/lightbulb";
import FlameIcon from "@lucide/svelte/icons/flame";
import DropletsIcon from "@lucide/svelte/icons/droplets";
import type { Component } from "svelte";
import type { SensorType, DeviceType } from "$lib/types";

export const sensorIcons: Record<SensorType, Component> = {
	temperature: ThermometerIcon,
	humidity: DropletIcon,
	co2: CloudIcon,
	light: SunIcon,
	soil_moisture: LeafIcon,
	vpd: GaugeIcon,
};

export const deviceIcons: Record<DeviceType, Component> = {
	fan: FanIcon,
	light: LightbulbIcon,
	heater: FlameIcon,
	pump: DropletsIcon,
	humidifier: DropletIcon,
};
