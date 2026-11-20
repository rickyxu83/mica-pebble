'use strict';

const assert = require('assert');

global.localStorage = {
    value: JSON.stringify({NOMINATIM_BASE_URL: 'https://geo.example.test'}),
    getItem() { return this.value; },
    setItem(key, value) { this.value = value; }
};

const requests = [];
global.XMLHttpRequest = class FakeXHR {
    constructor() {
        this.headers = {};
        this.readyState = 0;
        this.status = 200;
        this.responseText = JSON.stringify([{lat: '1.25', lon: '2.5'}]);
    }
    open(method, url) {
        this.method = method;
        this.url = url;
    }
    setRequestHeader(key, value) {
        this.headers[key] = value;
    }
    send() {
        this.sentAt = Date.now();
        requests.push(this);
        setTimeout(() => {
            this.readyState = 4;
            this.onload();
        }, 0);
    }
};

const nominatim = require('../app/src/pkjs/lib/nominatim');
function search(query) {
    return new Promise((resolve, reject) => {
        nominatim.search(query, (err, results) => err ? reject(err) : resolve(results));
    });
}

(async () => {
    await search('Paris');
    await search('London');
    assert.strictEqual(requests.length, 2, 'uncached queries make two requests');
    assert(requests[1].sentAt - requests[0].sentAt >= 1000, 'global request spacing is at least one second');
    assert.strictEqual(requests[0].headers['User-Agent'], 'Mica/0.1 (Pebble watch app)');
    assert(requests[0].url.startsWith('https://geo.example.test/search?'), 'custom endpoint');

    await search('  Paris  ');
    assert.strictEqual(requests.length, 2, 'normalized repeated query uses cache');
    console.log('NOMINATIM CLIENT TESTS PASSED');
})().catch(err => {
    console.error(err);
    process.exit(1);
});
