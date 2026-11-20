'use strict';

const assert = require('assert');
const path = require('path');

global.localStorage = {
    value: JSON.stringify({}),
    getItem() { return this.value; },
    setItem(key, value) { this.value = value; }
};

const root = path.resolve(__dirname, '../app/src/pkjs');
const config = require(path.join(root, 'config'));
const gemini = require(path.join(root, 'agent/gemini'));
const openai = require(path.join(root, 'agent/providers/openai_compat'));
const claude = require(path.join(root, 'agent/providers/anthropic'));
const calls = [];

[['gemini', gemini], ['openai', openai], ['claude', claude]].forEach(([name, provider]) => {
    provider.generate = (input, options, callback) => {
        calls.push({name, options});
        callback(null, {text: name, functionCalls: [], historyItems: []});
    };
});

const facade = require(path.join(root, 'agent/providers'));
['gemini', 'openai', 'claude'].forEach(name => {
    config.setSetting('LLM_PROVIDER', name);
    facade.generate([], {enableSearchGrounding: true}, () => {});
});

assert.deepStrictEqual(calls.map(call => call.name), ['gemini', 'openai', 'claude']);
assert.strictEqual(calls[0].options.enableSearchGrounding, true);
assert.strictEqual(calls[1].options.enableSearchGrounding, false);
assert.strictEqual(calls[2].options.enableSearchGrounding, false);
console.log('PROVIDER FACADE TESTS PASSED');
