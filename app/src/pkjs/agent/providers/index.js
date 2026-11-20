/**
 * Provider facade: routes generate() to the configured LLM provider.
 * All providers expose the same interface; the companionless runtime
 * and its tool loop are provider-agnostic.
 */

var config = require('../config');
var gemini = require('./gemini');

var PROVIDERS = {
    gemini: function() { return gemini; },
    openai: function() { return require('./providers/openai_compat'); },
    claude: function() { return require('./providers/anthropic'); }
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
    currentProvider().generate(input, options, callback);
};

exports.currentProviderName = function() {
    return config.getProvider();
};
