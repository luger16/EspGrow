import type { SystemInfo } from "$lib/types";
import { websocket } from "./websocket.svelte";

export const systemInfo = $state<{ data: SystemInfo | null }>({ data: null });

export function initSystemInfoWebSocket(): void {
	websocket.on("system_info", (data: unknown) => {
		const msg = data as SystemInfo;
		systemInfo.data = msg;
	});
}

export function requestSystemInfo(): void {
	websocket.send("get_system_info", {});
}
