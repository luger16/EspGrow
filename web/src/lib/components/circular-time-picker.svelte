<script lang="ts">
	import { settings } from "$lib/stores/settings.svelte";

	const STEP_MINUTES = 15;
	const TOTAL_MINUTES = 24 * 60;

	type Props = {
		startTime: string;
		endTime: string;
		onchange?: (startTime: string, endTime: string) => void;
	};
	let { startTime = $bindable(), endTime = $bindable(), onchange }: Props = $props();

	let is12h = $derived(settings.timeFormat === "12h");

	const size = 260;
	const cx = size / 2;
	const cy = size / 2;
	const radius = 88;
	const trackWidth = 20;
	const handleRadius = 12;
	const tickOuterRadius = radius + trackWidth / 2 + 6;

	let svgEl: SVGSVGElement | undefined = $state();
	let dragging: "start" | "end" | "arc" | null = $state(null);
	let arcDragOffset = $state(0);

	function timeToMinutes(time: string): number {
		const [h, m] = time.split(":").map(Number);
		return h * 60 + m;
	}

	function minutesToTime(minutes: number): string {
		const m = ((minutes % TOTAL_MINUTES) + TOTAL_MINUTES) % TOTAL_MINUTES;
		const hh = String(Math.floor(m / 60)).padStart(2, "0");
		const mm = String(m % 60).padStart(2, "0");
		return `${hh}:${mm}`;
	}

	function minutesToDegrees(minutes: number): number {
		return (minutes / TOTAL_MINUTES) * 360;
	}

	function degreesToMinutes(degrees: number): number {
		const norm = ((degrees % 360) + 360) % 360;
		return Math.round((norm / 360) * TOTAL_MINUTES / STEP_MINUTES) * STEP_MINUTES;
	}

	function polarToCartesian(angleDeg: number, r: number): { x: number; y: number } {
		const rad = ((angleDeg - 90) * Math.PI) / 180;
		return { x: cx + r * Math.cos(rad), y: cy + r * Math.sin(rad) };
	}

	function angleDiff(a: number, b: number): number {
		let diff = a - b;
		while (diff > 180) diff -= 360;
		while (diff < -180) diff += 360;
		return Math.abs(diff);
	}

	let startMinutes = $derived(timeToMinutes(startTime));
	let endMinutes = $derived(timeToMinutes(endTime));

	let startDeg = $derived(minutesToDegrees(startMinutes));
	let endDeg = $derived(minutesToDegrees(endMinutes));

	let startPos = $derived(polarToCartesian(startDeg, radius));
	let endPos = $derived(polarToCartesian(endDeg, radius));

	let arcPath = $derived.by(() => {
		const s = polarToCartesian(startDeg, radius);
		const e = polarToCartesian(endDeg, radius);

		let sweep = endDeg - startDeg;
		if (sweep < 0) sweep += 360;
		const largeArc = sweep > 180 ? 1 : 0;

		return `M ${s.x} ${s.y} A ${radius} ${radius} 0 ${largeArc} 1 ${e.x} ${e.y}`;
	});

	let durationMinutes = $derived.by(() => {
		let diff = endMinutes - startMinutes;
		if (diff <= 0) diff += TOTAL_MINUTES;
		return diff;
	});

	let durationText = $derived.by(() => {
		const hours = Math.floor(durationMinutes / 60);
		const mins = durationMinutes % 60;
		if (hours === 0) return `${mins}m`;
		if (mins === 0) return `${hours}h`;
		return `${hours}h ${mins}m`;
	});

	function formatDisplayTime(time: string): string {
		if (!is12h) return time;
		const [h, m] = time.split(":").map(Number);
		const ampm = h >= 12 ? "PM" : "AM";
		const hour12 = h % 12 || 12;
		return `${hour12}:${String(m).padStart(2, "0")} ${ampm}`;
	}

	function formatHourLabel(hour: number): string {
		if (!is12h) return String(hour);
		const labels: Record<number, string> = { 0: "12AM", 6: "6AM", 12: "12PM", 18: "6PM" };
		return labels[hour] ?? String(hour);
	}

	function getAngleFromEvent(event: PointerEvent): number {
		if (!svgEl) return 0;
		const rect = svgEl.getBoundingClientRect();
		const svgCx = rect.left + rect.width / 2;
		const svgCy = rect.top + rect.height / 2;
		const dx = event.clientX - svgCx;
		const dy = event.clientY - svgCy;
		let deg = (Math.atan2(dy, dx) * 180) / Math.PI + 90;
		if (deg < 0) deg += 360;
		return deg;
	}

	function isAngleOnArc(angle: number): boolean {
		let sweep = endDeg - startDeg;
		if (sweep < 0) sweep += 360;
		let fromStart = angle - startDeg;
		if (fromStart < 0) fromStart += 360;
		return fromStart > 0 && fromStart < sweep;
	}

	function resolveHandle(event: PointerEvent): "start" | "end" | "arc" | null {
		const angle = getAngleFromEvent(event);
		const distToStart = angleDiff(angle, startDeg);
		const distToEnd = angleDiff(angle, endDeg);
		const handleThreshold = 15;

		if (distToStart < handleThreshold || distToEnd < handleThreshold) {
			return distToStart <= distToEnd ? "start" : "end";
		}

		if (!svgEl) return null;
		const rect = svgEl.getBoundingClientRect();
		const scale = rect.width / size;
		const svgCx = rect.left + rect.width / 2;
		const svgCy = rect.top + rect.height / 2;
		const dx = event.clientX - svgCx;
		const dy = event.clientY - svgCy;
		const dist = Math.sqrt(dx * dx + dy * dy) / scale;
		const trackMin = radius - trackWidth / 2;
		const trackMax = radius + trackWidth / 2;

		if (dist >= trackMin && dist <= trackMax && isAngleOnArc(angle)) {
			return "arc";
		}

		return null;
	}

	function handlePointerDown(handle: "start" | "end", event: PointerEvent): void {
		event.preventDefault();
		event.stopPropagation();
		dragging = handle;
		(event.target as Element)?.setPointerCapture?.(event.pointerId);
	}

	function handleArcPointerDown(event: PointerEvent): void {
		const resolved = resolveHandle(event);
		if (!resolved) return;

		event.preventDefault();
		if (resolved === "start" || resolved === "end") {
			dragging = resolved;
		} else {
			dragging = "arc";
			arcDragOffset = getAngleFromEvent(event) - startDeg;
		}
		(event.target as Element)?.setPointerCapture?.(event.pointerId);
	}

	function handlePointerMove(event: PointerEvent): void {
		if (!dragging) return;
		const deg = getAngleFromEvent(event);
		const minutes = degreesToMinutes(deg);

		if (dragging === "start") {
			startTime = minutesToTime(minutes);
		} else if (dragging === "end") {
			endTime = minutesToTime(minutes);
		} else if (dragging === "arc") {
			const newStartDeg = deg - arcDragOffset;
			const newStartMin = degreesToMinutes(newStartDeg);
			const newEndMin = (newStartMin + durationMinutes) % TOTAL_MINUTES;
			startTime = minutesToTime(newStartMin);
			endTime = minutesToTime(newEndMin);
		}
		onchange?.(startTime, endTime);
	}

	function handlePointerUp(): void {
		dragging = null;
	}

	function handleKeyDown(handle: "start" | "end", event: KeyboardEvent): void {
		let delta = 0;
		if (event.key === "ArrowRight" || event.key === "ArrowUp") delta = STEP_MINUTES;
		else if (event.key === "ArrowLeft" || event.key === "ArrowDown") delta = -STEP_MINUTES;
		else return;

		event.preventDefault();
		const current = handle === "start" ? startMinutes : endMinutes;
		const next = ((current + delta) % TOTAL_MINUTES + TOTAL_MINUTES) % TOTAL_MINUTES;
		const time = minutesToTime(next);

		if (handle === "start") {
			startTime = time;
		} else {
			endTime = time;
		}
		onchange?.(startTime, endTime);
	}

	const hourTicks = Array.from({ length: 24 }, (_, i) => {
		const deg = (i / 24) * 360;
		const isMajor = i % 6 === 0;
		const innerR = radius + trackWidth / 2 + 2;
		const outerR = innerR + (isMajor ? 6 : 4);
		const p1 = polarToCartesian(deg, innerR);
		const p2 = polarToCartesian(deg, outerR);
		return { p1, p2, isMajor, hour: i };
	});

	let hourLabels = $derived([0, 6, 12, 18].map((h) => {
		const deg = (h / 24) * 360;
		const pos = polarToCartesian(deg, tickOuterRadius + (is12h ? 14 : 10));
		return { hour: h, ...pos };
	}));
</script>

<div class="flex flex-col items-center gap-2">
	<svg
		bind:this={svgEl}
		viewBox="0 0 {size} {size}"
		class="w-full max-w-[280px] touch-none select-none"
		role="group"
		aria-label="Schedule time range: {formatDisplayTime(startTime)} to {formatDisplayTime(endTime)}, active for {durationText}"
		onpointermove={handlePointerMove}
		onpointerup={handlePointerUp}
		onpointerleave={handlePointerUp}
	>
		<defs>
			<filter id="handle-shadow" x="-50%" y="-50%" width="200%" height="200%">
				<feDropShadow dx="0" dy="1" stdDeviation="1.5" flood-opacity="0.15" />
			</filter>
		</defs>

		<circle
			cx={cx}
			cy={cy}
			r={radius}
			fill="none"
			class="stroke-muted"
			stroke-width={trackWidth}
		/>

		<path
			d={arcPath}
			fill="none"
			class="stroke-primary/80 cursor-grab"
			class:cursor-grabbing={dragging === "arc"}
			stroke-width={trackWidth}
			stroke-linecap="round"
			role="presentation"
			onpointerdown={handleArcPointerDown}
		/>

		{#each hourTicks as tick (tick.hour)}
			<line
				x1={tick.p1.x}
				y1={tick.p1.y}
				x2={tick.p2.x}
				y2={tick.p2.y}
				class={tick.isMajor ? "stroke-muted-foreground/60" : "stroke-muted-foreground/30"}
				stroke-width={tick.isMajor ? 1.5 : 1}
			/>
		{/each}

		{#each hourLabels as label (label.hour)}
			<text
				x={label.x}
				y={label.y}
				text-anchor="middle"
				dominant-baseline="central"
				class="fill-muted-foreground font-medium {is12h ? 'text-[9px]' : 'text-[10px]'}"
			>
				{formatHourLabel(label.hour)}
			</text>
		{/each}

		<text
			x={cx}
			y={cy - 7}
			text-anchor="middle"
			dominant-baseline="central"
			class="fill-foreground font-semibold tabular-nums {is12h ? 'text-[11px]' : 'text-sm'}"
		>
			{formatDisplayTime(startTime)} – {formatDisplayTime(endTime)}
		</text>
		<text
			x={cx}
			y={cy + 10}
			text-anchor="middle"
			dominant-baseline="central"
			class="fill-muted-foreground text-[11px] font-medium"
		>
			{durationText}
		</text>

		<circle
			cx={startPos.x}
			cy={startPos.y}
			r={handleRadius}
			class="cursor-grab fill-background"
			class:cursor-grabbing={dragging === "start"}
			stroke="hsl(var(--ring))"
			stroke-width="2"
			filter="url(#handle-shadow)"
			role="slider"
			tabindex="0"
			aria-label="Start time"
			aria-valuenow={startMinutes}
			aria-valuemin={0}
			aria-valuemax={TOTAL_MINUTES}
			aria-valuetext={formatDisplayTime(startTime)}
			onpointerdown={(e) => handlePointerDown("start", e)}
			onkeydown={(e) => handleKeyDown("start", e)}
		/>
		<g transform="translate({startPos.x - 4}, {startPos.y - 4.5}) scale(0.375)" class="pointer-events-none">
			<path d="M5 5a2 2 0 0 1 3.008-1.728l11.997 6.998a2 2 0 0 1 .003 3.458l-12 7A2 2 0 0 1 5 19z" class="fill-muted-foreground" />
		</g>

		<circle
			cx={endPos.x}
			cy={endPos.y}
			r={handleRadius}
			class="cursor-grab fill-background"
			class:cursor-grabbing={dragging === "end"}
			stroke="hsl(var(--ring))"
			stroke-width="2"
			filter="url(#handle-shadow)"
			role="slider"
			tabindex="0"
			aria-label="End time"
			aria-valuenow={endMinutes}
			aria-valuemin={0}
			aria-valuemax={TOTAL_MINUTES}
			aria-valuetext={formatDisplayTime(endTime)}
			onpointerdown={(e) => handlePointerDown("end", e)}
			onkeydown={(e) => handleKeyDown("end", e)}
		/>
		<g transform="translate({endPos.x - 4}, {endPos.y - 4.5}) scale(0.375)" class="pointer-events-none">
			<rect width="18" height="18" x="3" y="3" rx="2" class="fill-muted-foreground" />
		</g>
	</svg>
</div>
