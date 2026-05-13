type MessageHandler = (data: unknown) => void;

import * as v from "valibot";
import { ServerToClientMessage } from "$lib/contract";

interface WebSocketState {
	connected: boolean;
	error: string | null;
	connectCount: number;
}

const state = $state<WebSocketState>({
	connected: false,
	error: null,
	connectCount: 0,
});

let ws: WebSocket | null = null;
let reconnectTimeout: ReturnType<typeof setTimeout> | null = null;
let shouldReconnect = true;
let reconnectDelay = 3000;
const RECONNECT_BASE = 3000;
const RECONNECT_MAX = 10000;
const HEARTBEAT_CHECK_MS = 5000;
const HEARTBEAT_TIMEOUT_MS = 15000;
const PENDING_QUEUE_MAX = 64;
const handlers = new Map<string, MessageHandler[]>();
let pendingMessages: Array<{ type: string; payload?: Record<string, unknown> }> = [];
let heartbeatInterval: ReturnType<typeof setInterval> | null = null;
let lastMessageTime = 0;
let visibilityListener: (() => void) | null = null;
let storedUrl: string | undefined;

function stopHeartbeat(): void {
	if (heartbeatInterval) {
		clearInterval(heartbeatInterval);
		heartbeatInterval = null;
	}
}

function startHeartbeat(): void {
	stopHeartbeat();
	heartbeatInterval = setInterval(() => {
		if (ws?.readyState === WebSocket.OPEN && Date.now() - lastMessageTime > HEARTBEAT_TIMEOUT_MS) {
			console.warn("WebSocket heartbeat timeout — reconnecting");
			ws.close();
		}
	}, HEARTBEAT_CHECK_MS);
}

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
	storedUrl = url;
	const wsUrl = url || getWebSocketUrl();
	if (!wsUrl) return;

	if (!url && import.meta.env.DEV && !import.meta.env.VITE_MOCK_WS) {
		return;
	}

	ws = new WebSocket(wsUrl);

	ws.onopen = () => {
		state.connected = true;
		state.error = null;
		state.connectCount++;
		reconnectDelay = RECONNECT_BASE;
		lastMessageTime = Date.now();
		startHeartbeat();
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
		stopHeartbeat();
		if (shouldReconnect && !isHidden()) {
			reconnectTimeout = setTimeout(() => connect(storedUrl), reconnectDelay);
			reconnectDelay = Math.min(reconnectDelay * 2, RECONNECT_MAX);
		}
	};

	ws.onerror = () => {
		state.error = "Connection failed";
	};

	ws.onmessage = (event) => {
		lastMessageTime = Date.now();
		let raw: unknown;
		try {
			raw = JSON.parse(event.data);
		} catch (err) {
			console.error("WebSocket parse error:", err);
			return;
		}

		const result = v.safeParse(ServerToClientMessage, raw);
		if (!result.success) {
			const summary = v.flatten(result.issues);
			if (import.meta.env.DEV) {
				console.error("WebSocket contract violation:", summary, raw);
				throw new Error(`WebSocket contract violation: ${JSON.stringify(summary)}`);
			}
			console.warn("WebSocket contract violation (dropped):", summary);
			return;
		}

		const msg = result.output;
		const typeHandlers = handlers.get(msg.type);
		if (typeHandlers) {
			const payload = (msg as { data?: unknown }).data;
			typeHandlers.forEach((handler) => handler(payload));
		}
	};
}

export function disconnect(): void {
	shouldReconnect = false;
	stopHeartbeat();
	teardownVisibility();
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

function isHidden(): boolean {
	return typeof document !== "undefined" && document.visibilityState === "hidden";
}

export function setupVisibility(): () => void {
	if (typeof document === "undefined") return () => {};
	teardownVisibility();
	const handler = () => {
		if (document.visibilityState === "hidden") {
			stopHeartbeat();
			if (reconnectTimeout) {
				clearTimeout(reconnectTimeout);
				reconnectTimeout = null;
			}
		} else {
			lastMessageTime = Date.now();
			if (ws?.readyState === WebSocket.OPEN) {
				startHeartbeat();
				send("ping");
			} else {
				reconnectDelay = RECONNECT_BASE;
				connect(storedUrl);
			}
		}
	};
	document.addEventListener("visibilitychange", handler);
	visibilityListener = () => document.removeEventListener("visibilitychange", handler);
	return teardownVisibility;
}

function teardownVisibility(): void {
	if (visibilityListener) {
		visibilityListener();
		visibilityListener = null;
	}
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
		if (pendingMessages.length > PENDING_QUEUE_MAX) {
			pendingMessages.splice(0, pendingMessages.length - PENDING_QUEUE_MAX);
		}
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
	get connectCount() {
		return state.connectCount;
	},
	connect,
	disconnect,
	send,
	on,
	setupVisibility,
};
