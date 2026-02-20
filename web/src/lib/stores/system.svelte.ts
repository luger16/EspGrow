import type { SystemInfo } from "$lib/types";
import { websocket } from "./websocket.svelte";

export const systemInfo = $state<{ data: SystemInfo | null }>({ data: null });

export function initSystemInfoWebSocket(): void {
	websocket.on("system_info", (data: unknown) => {
		if (data && typeof data === "object" && "uptime" in data) {
			systemInfo.data = data as SystemInfo;
		}
	});
}

export function requestSystemInfo(): void {
	websocket.send("get_system_info");
}
