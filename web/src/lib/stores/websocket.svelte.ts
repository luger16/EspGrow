type MessageHandler = (data: unknown) => void;

interface WebSocketState {
	connected: boolean;
	error: string | null;
}

const state = $state<WebSocketState>({
	connected: false,
	error: null,
});

let ws: WebSocket | null = null;
let reconnectTimeout: ReturnType<typeof setTimeout> | null = null;
const handlers = new Map<string, MessageHandler[]>();
let pendingMessages: Array<{ type: string; payload?: Record<string, unknown> }> = [];

function getWebSocketUrl(): string {
	if (typeof window === "undefined") return "";
	const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
	return `${protocol}//${window.location.host}/ws`;
}

export function connect(url?: string): void {
	if (ws?.readyState === WebSocket.OPEN) return;

	const wsUrl = url || getWebSocketUrl();
	if (!wsUrl) return;

	const isDevModeWithoutExplicitUrl = !url && import.meta.env.DEV;
	if (isDevModeWithoutExplicitUrl) {
		return;
	}

	ws = new WebSocket(wsUrl);

	ws.onopen = () => {
		state.connected = true;
		state.error = null;
		for (const msg of pendingMessages) {
			ws!.send(JSON.stringify({ type: msg.type, ...msg.payload }));
		}
		pendingMessages = [];
	};

	ws.onclose = () => {
		state.connected = false;
		reconnectTimeout = setTimeout(() => connect(url), 3000);
	};

	ws.onerror = () => {
		state.error = "Connection failed";
	};

	ws.onmessage = (event) => {
		try {
			const data = JSON.parse(event.data);
			const type = data.type as string;
			const typeHandlers = handlers.get(type);
			if (typeHandlers) {
				typeHandlers.forEach((handler) => handler(data));
			}
		} catch (err) {
			console.error("WebSocket parse error:", err);
		}
	};
}

export function disconnect(): void {
	if (reconnectTimeout) {
		clearTimeout(reconnectTimeout);
		reconnectTimeout = null;
	}
	if (ws) {
		ws.close();
		ws = null;
	}
	state.connected = false;
}

export function send(type: string, payload?: Record<string, unknown>): void {
	if (ws?.readyState === WebSocket.OPEN) {
		ws.send(JSON.stringify({ type, ...payload }));
	} else {
		pendingMessages.push({ type, payload });
	}
}

export function on(type: string, handler: MessageHandler): () => void {
	if (!handlers.has(type)) {
		handlers.set(type, []);
	}
	handlers.get(type)!.push(handler);

	return () => {
		const typeHandlers = handlers.get(type);
		if (typeHandlers) {
			const index = typeHandlers.indexOf(handler);
			if (index !== -1) typeHandlers.splice(index, 1);
		}
	};
}

export const websocket = {
	get connected() {
		return state.connected;
	},
	get error() {
		return state.error;
	},
	connect,
	disconnect,
	send,
	on,
};
