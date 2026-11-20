/**
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 *
 * Anthropic Claude provider (/v1/messages).
 * Same neutral history contract as providers/openai_compat.js.
 */

var config = require('../../config');

var API_URL = require('../../config').getSetting('CLAUDE_BASE_URL', '') || 'https://api.anthropic.com/v1/messages';
var API_VERSION = '2023-06-01';
var REQUEST_TIMEOUT_MS = 60000;

function extractText(content) {
    if (typeof content === 'string') {
        return content;
    }
    if (!Array.isArray(content)) {
        return String(content || '');
    }
    return content.map(function(part) {
        return part && part.text ? part.text : '';
    }).join('\n');
}

function resultToText(result) {
    if (!Array.isArray(result) || result.length === 0) {
        return '{}';
    }
    var first = result[0];
    if (first && first.type === 'text') {
        return first.text || '{}';
    }
    return JSON.stringify(result);
}

function buildMessages(input) {
    var messages = [];
    (input || []).forEach(function(item) {
        if (!item) {
            return;
        }
        if (item.role === 'user' || item.role === 'assistant') {
            // Native message from our own historyItems. Convert content blocks.
            if (typeof item.content === 'string') {
                messages.push(item);
            } else {
                messages.push(item);
            }
            return;
        }
        if (item.type === 'user_input') {
            messages.push({role: 'user', content: extractText(item.content)});
            return;
        }
        if (item.type === 'assistant_text') {
            messages.push({role: 'assistant', content: item.text || ''});
            return;
        }
        if (item.type === 'function_result') {
            messages.push({
                role: 'user',
                content: [{
                    type: 'tool_result',
                    tool_use_id: item.call_id || item.name,
                    content: resultToText(item.result)
                }]
            });
            return;
        }
        if (item.type === 'function_call') {
            messages.push({
                role: 'assistant',
                content: [{
                    type: 'tool_use',
                    id: item.id || ('toolu_' + item.name),
                    name: item.name,
                    input: item.arguments || {}
                }]
            });
        }
    });
    return messages;
}

function buildTools(tools) {
    if (!tools || tools.length === 0) {
        return undefined;
    }
    var out = [];
    tools.forEach(function(tool) {
        if (tool.type !== 'function' || !tool.name) {
            return;
        }
        out.push({
            name: tool.name,
            description: tool.description || '',
            input_schema: (tool.parameters && typeof tool.parameters === 'object') ?
                tool.parameters : {type: 'object', properties: {}}
        });
    });
    return out.length > 0 ? out : undefined;
}

function parseResponse(response) {
    var text = '';
    var functionCalls = [];
    (response.content || []).forEach(function(block) {
        if (block.type === 'text' && block.text) {
            text += block.text;
        }
        if (block.type === 'tool_use' && block.name) {
            functionCalls.push({
                id: block.id,
                name: block.name,
                arguments: block.input || {}
            });
        }
    });
    return {
        text: text,
        functionCalls: functionCalls,
        historyItems: [{role: 'assistant', content: response.content || []}],
        raw: response
    };
}

function parseError(status, responseText, model) {
    var message = responseText;
    try {
        var parsed = JSON.parse(responseText);
        if (parsed.error && parsed.error.message) {
            message = parsed.error.message;
        }
    } catch (e) {
        // keep raw text
    }
    message = String(message || 'unknown error');
    if (message.length > 180) {
        message = message.substring(0, 177) + '...';
    }
    var err = new Error('Claude ' + model + ' failed (' + status + '): ' + message);
    err.status = status;
    return err;
}

exports.generate = function(input, options, callback) {
    var apiKey = config.getClaudeApiKey();
    if (!apiKey) {
        callback(new Error('Claude API key is not configured. Open Mica settings and add an Anthropic API key.'));
        return;
    }
    options = options || {};
    var model = config.getClaudeModel();
    var body = {
        model: model,
        max_tokens: 2048,
        messages: buildMessages(input)
    };
    if (options.systemInstruction) {
        body.system = options.systemInstruction;
    }
    var tools = buildTools(options.tools);
    if (tools) {
        body.tools = tools;
    }

    var finished = false;
    var req = new XMLHttpRequest();
    function finish(err, value) {
        if (finished) {
            return;
        }
        finished = true;
        callback(err, value);
    }
    req.open('POST', API_URL, true);
    req.timeout = REQUEST_TIMEOUT_MS;
    req.setRequestHeader('Content-Type', 'application/json');
    req.setRequestHeader('x-api-key', apiKey);
    req.setRequestHeader('anthropic-version', API_VERSION);
    // Required for browser/WebView-originated requests (PKJS runs in a WebView).
    req.setRequestHeader('anthropic-dangerous-direct-browser-access', 'true');
    req.onload = function() {
        if (req.readyState !== 4) {
            return;
        }
        if (req.status < 200 || req.status >= 300) {
            finish(parseError(req.status, req.responseText, model));
            return;
        }
        try {
            finish(null, parseResponse(JSON.parse(req.responseText)));
        } catch (e) {
            finish(e);
        }
    };
    req.onerror = function() {
        finish(new Error('Could not reach api.anthropic.com. Check network.'));
    };
    req.ontimeout = function() {
        finish(new Error('Claude ' + model + ' timed out. Please try again.'));
    };
    req.send(JSON.stringify(body));
};
