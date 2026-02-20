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
let shouldReconnect = true;
const handlers = new Map<string, MessageHandler[]>();
let pendingMessages: Array<{ type: string; payload?: Record<string, unknown> }> = [];

function getWebSocketUrl(): string {
	if (typeof window === "undefined") return "";
	if (import.meta.env.VITE_MOCK_WS) {
		return `ws://${window.location.hostname}:${import.meta.env.VITE_MOCK_WS}`;
	}
	const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
	return `${protocol}//${window.location.host}/ws`;
}

export function connect(url?: string): void {
	if (ws?.readyState === WebSocket.OPEN || ws?.readyState === WebSocket.CONNECTING) return;

	shouldReconnect = true;
	const wsUrl = url || getWebSocketUrl();
	if (!wsUrl) return;

	if (!url && import.meta.env.DEV && !import.meta.env.VITE_MOCK_WS) {
		return;
	}

	ws = new WebSocket(wsUrl);

	ws.onopen = () => {
		state.connected = true;
		state.error = null;
		for (const msg of pendingMessages) {
			const frame: Record<string, unknown> = { type: msg.type };
			if (msg.payload && Object.keys(msg.payload).length > 0) {
				frame.data = msg.payload;
			}
			ws!.send(JSON.stringify(frame));
		}
		pendingMessages = [];
	};

	ws.onclose = () => {
		state.connected = false;
		ws = null;
		if (shouldReconnect) {
			reconnectTimeout = setTimeout(() => connect(url), 3000);
		}
	};

	ws.onerror = () => {
		state.error = "Connection failed";
	};

	ws.onmessage = (event) => {
		try {
			const msg = JSON.parse(event.data);
			if (typeof msg !== "object" || msg === null || !("type" in msg)) return;
			
			const type = (msg as { type: unknown }).type;
			if (typeof type !== "string") return;

			const typeHandlers = handlers.get(type);
			if (typeHandlers) {
				const payload = (msg as Record<string, unknown>).data ?? msg;
				typeHandlers.forEach((handler) => handler(payload));
			}
		} catch (err) {
			console.error("WebSocket parse error:", err);
		}
	};
}

export function disconnect(): void {
	shouldReconnect = false;
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
	const frame: Record<string, unknown> = { type };
	if (payload && Object.keys(payload).length > 0) {
		frame.data = payload;
	}
	if (ws?.readyState === WebSocket.OPEN) {
		ws.send(JSON.stringify(frame));
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
