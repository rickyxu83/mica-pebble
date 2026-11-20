/**
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 *
 * Shared, user-triggered Nominatim client. One queue is shared by weather and
 * maps so the whole app stays below the public service's 1 request/second cap.
 */

var config = require('../config');

var MIN_INTERVAL_MS = 1100;
var CACHE_LIMIT = 50;
var queue = [];
var cache = {};
var cacheKeys = [];
var lastStartedAt = 0;
var active = false;

function cleanQuery(query) {
    return String(query || '').replace(/\s+/g, ' ').trim();
}

exports.search = function(query, callback) {
    var clean = cleanQuery(query);
    if (!clean) {
        callback(new Error('A place name is required.'));
        return;
    }
    var key = clean.toLowerCase();
    if (Object.prototype.hasOwnProperty.call(cache, key)) {
        setTimeout(function() { callback(null, cache[key]); }, 0);
        return;
    }
    queue.push({key: key, query: clean, callback: callback});
    pump();
};

function pump() {
    if (active || queue.length === 0) {
        return;
    }
    active = true;
    var wait = Math.max(0, lastStartedAt + MIN_INTERVAL_MS - Date.now());
    setTimeout(startNext, wait);
}

function startNext() {
    var item = queue.shift();
    if (!item) {
        active = false;
        return;
    }
    lastStartedAt = Date.now();
    var base = config.getNominatimBaseUrl();
    var url = base + '/search?format=json&limit=1&q=' + encodeURIComponent(item.query);
    var req = new XMLHttpRequest();
    var finished = false;

    function finish(err, results) {
        if (finished) {
            return;
        }
        finished = true;
        if (!err) {
            remember(item.key, results);
        }
        active = false;
        pump();
        item.callback(err, results);
    }

    req.open('GET', url, true);
    req.timeout = 12000;
    req.setRequestHeader('Accept', 'application/json');
    try {
        // PebbleKit JS permits this; browser WebViews may forbid User-Agent.
        req.setRequestHeader('User-Agent', 'Mica/0.1 (Pebble watch app)');
    } catch (e) {
        console.log('Nominatim User-Agent is controlled by the host runtime.');
    }
    req.onload = function() {
        if (req.readyState !== 4) {
            return;
        }
        if (req.status < 200 || req.status >= 300) {
            finish(new Error('HTTP ' + req.status + ': ' + String(req.responseText || '').substring(0, 120)));
            return;
        }
        try {
            finish(null, JSON.parse(req.responseText));
        } catch (e) {
            finish(e);
        }
    };
    req.onerror = function() { finish(new Error('network error')); };
    req.ontimeout = function() { finish(new Error('request timed out')); };
    req.send();
}

function remember(key, results) {
    if (cacheKeys.length >= CACHE_LIMIT) {
        delete cache[cacheKeys.shift()];
    }
    cache[key] = results;
    cacheKeys.push(key);
}
