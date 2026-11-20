/**
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 *
 * OpenAI-compatible provider (/v1/chat/completions).
 *
 * Works with OpenAI itself and any compatible endpoint:
 * DeepSeek, Moonshot/Kimi, Qwen, OpenRouter, Ollama, LM Studio, ...
 *
 * Speaks the same interface as agent/gemini.js:
 *   generate(input, options, callback)
 *     input:   array of neutral history items
 *              ({type:'user_input', content:[{type:'text',text}]},
 *               {type:'assistant_text', text},
 *               {type:'function_call', id, name, arguments},
 *               {type:'function_result', name, call_id, result:[{type:'text',text}]})
 *     options: {enableSearchGrounding, tools, systemInstruction}
 *     callback(err, {text, functionCalls:[{id,name,arguments}], historyItems, raw})
 *
 * Apache 2.0 (same as upstream Bobby/Billy).
 */

var config = require('../../config');

var REQUEST_TIMEOUT_MS = 60000;
var MAX_TOOL_ROUNDS_NOTE = 3;

function endpointBase() {
    var base = String(config.getOpenaiBaseUrl() || '').trim();
    if (!base) {
        base = 'https://api.openai.com/v1';
    }
    // Tolerate users pasting a full ".../chat/completions" URL.
    base = base.replace(/\/+$/, '');
    base = base.replace(/\/chat\/completions$/, '');
    return base;
}

function buildMessages(input, systemInstruction) {
    var messages = [];
    if (systemInstruction) {
        messages.push({role: 'system', content: systemInstruction});
    }
    (input || []).forEach(function(item) {
        if (!item) {
            return;
        }
        if (item.role === 'assistant' || item.role === 'user' || item.role === 'system') {
            // Already a native message (our own historyItems).
            messages.push(item);
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
                role: 'tool',
                tool_call_id: item.call_id || item.name,
                name: item.name,
                content: resultToText(item.result)
            });
            return;
        }
        if (item.type === 'function_call') {
            messages.push(assistantToolCallMessage([item]));
            return;
        }
        if (item.type === 'text' && item.text) {
            messages.push({role: 'user', content: item.text});
        }
    });
    return messages;
}

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

function assistantToolCallMessage(calls) {
    return {
        role: 'assistant',
        content: null,
        tool_calls: calls.map(function(call) {
            return {
                id: call.id || ('call_' + call.name),
                type: 'function',
                function: {
                    name: call.name,
                    arguments: typeof call.arguments === 'string' ?
                        call.arguments : JSON.stringify(call.arguments || {})
                }
            };
        })
    };
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
            type: 'function',
            function: {
                name: tool.name,
                description: tool.description || '',
                parameters: (tool.parameters && typeof tool.parameters === 'object') ?
                    tool.parameters : {type: 'object', properties: {}}
            }
        });
    });
    return out.length > 0 ? out : undefined;
}

function parseResponse(response) {
    var choice = response.choices && response.choices[0];
    if (!choice || !choice.message) {
        throw new Error('Empty response from OpenAI-compatible endpoint.');
    }
    var message = choice.message;
    var functionCalls = [];
    (message.tool_calls || []).forEach(function(call) {
        if (!call.function || !call.function.name) {
            return;
        }
        var args = {};
        try {
            args = call.function.arguments ? JSON.parse(call.function.arguments) : {};
        } catch (e) {
            args = {};
        }
        functionCalls.push({
            id: call.id,
            name: call.function.name,
            arguments: args
        });
    });
    return {
        text: message.content || '',
        functionCalls: functionCalls,
        historyItems: [message],
        raw: response
    };
}

function parseError(status, responseText, model) {
    var message = responseText;
    try {
        var parsed = JSON.parse(responseText);
        if (parsed.error) {
            message = parsed.error.message || message;
        } else if (parsed.message) {
            message = parsed.message;
        }
    } catch (e) {
        // keep raw text
    }
    message = String(message || 'unknown error');
    if (message.length > 180) {
        message = message.substring(0, 177) + '...';
    }
    var err = new Error(model + ' request failed (' + status + '): ' + message);
    err.status = status;
    return err;
}

exports.generate = function(input, options, callback) {
    var apiKey = config.getOpenaiApiKey();
    if (!apiKey) {
        callback(new Error('API key is not configured. Open Mica settings and add your provider API key.'));
        return;
    }
    options = options || {};
    var model = config.getOpenaiModel();
    var body = {
        model: model,
        messages: buildMessages(input, options.systemInstruction),
        max_tokens: 2048,
        temperature: 0.7
    };
    var tools = buildTools(options.tools);
    if (tools) {
        body.tools = tools;
        body.tool_choice = 'auto';
    }
    if (options.enableSearchGrounding) {
        // Native web_search is an OpenAI Responses-API feature; most compatible
        // endpoints do not support it. If the user configured a search-capable
        // deployment they can expose it as a tool instead.
        body.wrist_search_requested = undefined;
        delete body.wrist_search_requested;
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
    req.open('POST', endpointBase() + '/chat/completions', true);
    req.timeout = REQUEST_TIMEOUT_MS;
    req.setRequestHeader('Content-Type', 'application/json');
    req.setRequestHeader('Authorization', 'Bearer ' + apiKey);
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
        finish(new Error('Could not reach ' + endpointBase() + '. Check the endpoint URL and network.'));
    };
    req.ontimeout = function() {
        finish(new Error(model + ' timed out. Try a smaller/faster model.'));
    };
    req.send(JSON.stringify(body));
};
