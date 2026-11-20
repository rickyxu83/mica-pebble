// Minimal XMLHttpRequest shim over node:http for provider smoke tests.
'use strict';
const http = require('http');

module.exports = class XMLHttpRequest {
    open(method, url) {
        this._method = method;
        this._url = url;
        this._headers = {};
        this.readyState = 1;
    }
    setRequestHeader(k, v) { this._headers[k] = v; }
    send(body) {
        const req = http.request(this._url, {
            method: this._method, headers: this._headers
        }, res => {
            let text = '';
            res.on('data', c => text += c);
            res.on('end', () => {
                this.readyState = 4;
                this.status = res.statusCode;
                this.responseText = text;
                if (this.onload) this.onload();
            });
        });
        req.on('error', () => { if (this.onerror) this.onerror(); });
        req.end(body || '');
    }
};
