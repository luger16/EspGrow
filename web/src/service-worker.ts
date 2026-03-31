/// <reference no-default-lib="true"/>
/// <reference lib="esnext" />
/// <reference lib="webworker" />
/// <reference types="@sveltejs/kit" />

import { build, files, version } from "$service-worker";

const sw = globalThis as unknown as ServiceWorkerGlobalScope;

const CACHE = `espgrow-${version}`;
const PRECACHE = [...build, ...files];

sw.addEventListener("install", (event) => {
	event.waitUntil(
		caches.open(CACHE).then((cache) => cache.addAll(PRECACHE)),
	);
	sw.skipWaiting();
});

sw.addEventListener("activate", (event) => {
	event.waitUntil(
		caches.keys().then((keys) =>
			Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))),
		),
	);
	sw.clients.claim();
});

sw.addEventListener("fetch", (event) => {
	const { request } = event;
	const url = new URL(request.url);

	if (request.method !== "GET") return;
	if (url.origin !== sw.location.origin) return;

	// API calls must always hit ESP32 live
	if (url.pathname.startsWith("/api/")) return;
	if (request.headers.get("upgrade") === "websocket") return;

	// Build assets have content hashes — serve from cache
	if (PRECACHE.includes(url.pathname)) {
		event.respondWith(
			caches.match(request).then((cached) => cached ?? fetch(request)),
		);
		return;
	}

	// SPA navigation: network-first, fall back to cached index.html
	if (request.mode === "navigate") {
		event.respondWith(
			fetch(request)
				.then((response) => {
					const clone = response.clone();
					caches.open(CACHE).then((cache) => cache.put(request, clone));
					return response;
				})
				.catch(() =>
					caches.match(request).then((cached) =>
						cached ?? caches.match("/"),
					) as Promise<Response>,
				),
		);
		return;
	}
});
