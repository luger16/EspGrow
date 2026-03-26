<script lang="ts">
	const STEP_MINUTES = 15;
	const TOTAL_MINUTES = 24 * 60;

	type Props = {
		startTime: string;
		endTime: string;
		onchange?: (startTime: string, endTime: string) => void;
	};
	let { startTime = $bindable(), endTime = $bindable(), onchange }: Props = $props();

	const size = 240;
	const cx = size / 2;
	const cy = size / 2;
	const radius = 90;
	const trackWidth = 24;
	const handleRadius = 14;
	const tickOuterRadius = radius + trackWidth / 2 + 8;

	let svgEl: SVGSVGElement | undefined = $state();
	let dragging: "start" | "end" | null = $state(null);

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

	let durationText = $derived.by(() => {
		let diff = endMinutes - startMinutes;
		if (diff <= 0) diff += TOTAL_MINUTES;
		const hours = Math.floor(diff / 60);
		const mins = diff % 60;
		if (hours === 0) return `${mins}m`;
		if (mins === 0) return `${hours}h`;
		return `${hours}h ${mins}m`;
	});

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

	function handlePointerDown(handle: "start" | "end", event: PointerEvent): void {
		event.preventDefault();
		dragging = handle;
		(event.target as Element)?.setPointerCapture?.(event.pointerId);
	}

	function handlePointerMove(event: PointerEvent): void {
		if (!dragging) return;
		const deg = getAngleFromEvent(event);
		const minutes = degreesToMinutes(deg);
		const time = minutesToTime(minutes);

		if (dragging === "start") {
			startTime = time;
		} else {
			endTime = time;
		}
		onchange?.(startTime, endTime);
	}

	function handlePointerUp(): void {
		dragging = null;
	}

	const hourTicks = Array.from({ length: 24 }, (_, i) => {
		const deg = (i / 24) * 360;
		const isMajor = i % 6 === 0;
		const innerR = radius + trackWidth / 2 + 2;
		const outerR = innerR + (isMajor ? 8 : 5);
		const p1 = polarToCartesian(deg, innerR);
		const p2 = polarToCartesian(deg, outerR);
		return { p1, p2, isMajor, hour: i };
	});

	const hourLabels = [0, 6, 12, 18].map((h) => {
		const deg = (h / 24) * 360;
		const pos = polarToCartesian(deg, tickOuterRadius + 12);
		return { hour: h, ...pos };
	});
</script>

<div class="flex flex-col items-center gap-2">
	<svg
		bind:this={svgEl}
		viewBox="0 0 {size} {size}"
		class="w-full max-w-[220px] touch-none select-none"
		role="group"
		aria-label="Schedule time range: {startTime} to {endTime}, active for {durationText}"
		onpointermove={handlePointerMove}
		onpointerup={handlePointerUp}
		onpointerleave={handlePointerUp}
	>
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
			class="stroke-primary"
			stroke-width={trackWidth}
			stroke-linecap="round"
		/>

		{#each hourTicks as tick (tick.hour)}
			<line
				x1={tick.p1.x}
				y1={tick.p1.y}
				x2={tick.p2.x}
				y2={tick.p2.y}
				class={tick.isMajor ? "stroke-foreground" : "stroke-muted-foreground/50"}
				stroke-width={tick.isMajor ? 1.5 : 1}
			/>
		{/each}

		{#each hourLabels as label (label.hour)}
			<text
				x={label.x}
				y={label.y}
				text-anchor="middle"
				dominant-baseline="central"
				class="fill-muted-foreground text-[10px]"
			>
				{label.hour}
			</text>
		{/each}

		<text
			x={cx}
			y={cy - 8}
			text-anchor="middle"
			dominant-baseline="central"
			class="fill-foreground text-sm font-medium"
		>
			{startTime}–{endTime}
		</text>
		<text
			x={cx}
			y={cy + 10}
			text-anchor="middle"
			dominant-baseline="central"
			class="fill-muted-foreground text-[11px]"
		>
			{durationText}
		</text>

		<circle
			cx={startPos.x}
			cy={startPos.y}
			r={handleRadius}
			class="fill-primary cursor-grab stroke-background"
			class:cursor-grabbing={dragging === "start"}
			stroke-width="3"
			role="slider"
			tabindex="0"
			aria-label="Start time"
			aria-valuenow={startMinutes}
			aria-valuemin={0}
			aria-valuemax={TOTAL_MINUTES}
			aria-valuetext={startTime}
			onpointerdown={(e) => handlePointerDown("start", e)}
		/>
		<text
			x={startPos.x}
			y={startPos.y}
			text-anchor="middle"
			dominant-baseline="central"
			class="pointer-events-none fill-primary-foreground text-[8px] font-bold"
		>
			S
		</text>

		<circle
			cx={endPos.x}
			cy={endPos.y}
			r={handleRadius}
			class="fill-primary cursor-grab stroke-background"
			class:cursor-grabbing={dragging === "end"}
			stroke-width="3"
			role="slider"
			tabindex="0"
			aria-label="End time"
			aria-valuenow={endMinutes}
			aria-valuemin={0}
			aria-valuemax={TOTAL_MINUTES}
			aria-valuetext={endTime}
			onpointerdown={(e) => handlePointerDown("end", e)}
		/>
		<text
			x={endPos.x}
			y={endPos.y}
			text-anchor="middle"
			dominant-baseline="central"
			class="pointer-events-none fill-primary-foreground text-[8px] font-bold"
		>
			E
		</text>
	</svg>
</div>
