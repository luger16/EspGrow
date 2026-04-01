import type { ClimateConfig, ClimatePhase, PhaseTargets } from "$lib/types";

const PHASE_TARGETS: Record<ClimatePhase, PhaseTargets> = {
	seedling: {
		temp: { day: 25, night: 21 },
		humidity: { day: 70, night: 75 },
		vpd: { day: 0.6, night: 0.5 },
		co2: { day: 500, night: 500 },
		dli: 15,
	},
	veg: {
		temp: { day: 25, night: 20 },
		humidity: { day: 60, night: 65 },
		vpd: { day: 1.0, night: 0.8 },
		co2: { day: 1000, night: 1000 },
		dli: 30,
	},
	flower: {
		temp: { day: 23, night: 18 },
		humidity: { day: 48, night: 52 },
		vpd: { day: 1.2, night: 1.0 },
		co2: { day: 1200, night: 1200 },
		dli: 40,
	},
	dry: {
		temp: { day: 20, night: 18 },
		humidity: { day: 50, night: 55 },
		vpd: { day: 1.2, night: 1.0 },
		co2: { day: 500, night: 500 },
		dli: 0,
	},
};

export const DEFAULT_PHASE_TARGETS: Record<ClimatePhase, PhaseTargets> = PHASE_TARGETS;

export const DEFAULT_CLIMATE_CONFIG: ClimateConfig = {
	activePhase: "veg",
	phases: PHASE_TARGETS,
	dayNightMode: "auto",
	lightThreshold: 50,
};
