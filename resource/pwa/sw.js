'use strict';

const CACHE_NAME = 'precess-wasm-@CACHE_ID@';

const PRECACHE_URLS = [
    './',
    'index.html',
    'qtloader.js',
    '@MYNAME@.js',
    '@MYNAME@.wasm',
    '@MYNAME@.data',
    '@MYNAME@.worker.js',
    'manifest.json',
    'icons/icon-192.png',
    'icons/icon-512.png',
    'icons/maskable-192.png',
    'icons/maskable-512.png',
    'icons/apple-touch-icon.png',
];

self.addEventListener('install', (event) => {
    self.skipWaiting();
    event.waitUntil((async () => {
        const cache = await caches.open(CACHE_NAME);
        await Promise.allSettled(PRECACHE_URLS.map((url) => cache.add(url)));
    })());
});

self.addEventListener('activate', (event) => {
    event.waitUntil((async () => {
        const names = await caches.keys();
        await Promise.all(names
            .filter((name) => name !== CACHE_NAME)
            .map((name) => caches.delete(name)));
        await self.clients.claim();
    })());
});

self.addEventListener('fetch', (event) => {
    const request = event.request;
    if (request.method !== 'GET')
        return;
    const url = new URL(request.url);
    if (url.origin !== self.location.origin)
        return;

    if (request.mode === 'navigate') {
        event.respondWith((async () => {
            try {
                const response = await fetch(request);
                if (response.ok) {
                    // 响应连同 COOP/COEP 头一并入库，多线程构建离线启动时才能保持跨源隔离
                    const cache = await caches.open(CACHE_NAME);
                    cache.put('./', response.clone());
                }
                return response;
            } catch (error) {
                const cache = await caches.open(CACHE_NAME);
                const cached = (await cache.match('./')) ?? (await cache.match('index.html'));
                return cached ?? new Response(
                    '<!doctype html><meta charset="utf-8"><p>当前离线且尚未缓存本应用，请联网后重新打开。</p>',
                    {status: 503, headers: {'Content-Type': 'text/html; charset=utf-8'}});
            }
        })());
        return;
    }

    event.respondWith((async () => {
        const cache = await caches.open(CACHE_NAME);
        const cached = await cache.match(request);
        if (cached)
            return cached;
        const response = await fetch(request);
        if (response.ok)
            cache.put(request, response.clone());
        return response;
    })());
});
