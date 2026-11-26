/**
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 *
 * Provider facade: routes generate() to the configured LLM provider.
 * All providers expose the same interface; the companionless runtime
 * and its tool loop are provider-agnostic.
 */

var config = require('../../config');
var gemini = require('../gemini');
var diagnostics = require('../diagnostics');

var PROVIDERS = {
    gemini: function() { return gemini; },
    openai: function() { return require('./openai_compat'); },
    claude: function() { return require('./anthropic'); }
};

function currentProvider() {
    var id = config.getProvider();
    return (PROVIDERS[id] || PROVIDERS.gemini)();
}

exports.generate = function(input, options, callback) {
    options = options || {};
    // Native Google Search grounding only exists on Gemini.
    if (config.getProvider() !== 'gemini') {
        options.enableSearchGrounding = false;
    }
    var providerId = config.getProvider();
    currentProvider().generate(input, options, function(err, response) {
        if (err) {
            diagnostics.record('provider_error', providerId +
                (err.status ? ' HTTP ' + err.status + ': ' : ': ') +
                (err.message || String(err)));
        }
        callback(err, response);
    });
};

exports.currentProviderName = function() {
    return config.getProvider();
};
