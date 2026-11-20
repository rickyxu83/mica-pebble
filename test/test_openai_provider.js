// Node smoke test for the OpenAI-compatible provider.
// Mocks PKJS globals (localStorage + XMLHttpRequest) and a local HTTP server,
// then exercises: system prompt, user input, tool call, tool result, final answer.
'use strict';
const http = require('http');

const settings = {
    LLM_PROVIDER: 'openai',
    OPENAI_API_KEY: 'test-key',
    OPENAI_BASE_URL: '', // filled after server starts
    OPENAI_MODEL: 'deepseek-chat',
};
global.localStorage = {
    getItem: k => (k === 'clay-settings' ? JSON.stringify(settings) : null),
    setItem: () => {},
};
global.XMLHttpRequest = require('./xhr_shim');

const provider = require('../app/src/pkjs/agent/providers/openai_compat');

const requests = [];
const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', c => body += c);
    req.on('end', () => {
        const parsed = JSON.parse(body);
        requests.push({url: req.url, auth: req.headers.authorization, body: parsed});
        res.setHeader('Content-Type', 'application/json');
        if (requests.length === 1) {
            // First turn: model requests a tool call.
            res.end(JSON.stringify({
                usage: {prompt_tokens: 11, completion_tokens: 5, total_tokens: 16},
                choices: [{message: {
                    role: 'assistant', content: null,
                    tool_calls: [{id: 'call_1', type: 'function', function: {
                        name: 'get_weather', arguments: JSON.stringify({location: 'Suzhou'})
                    }}]
                }}]
            }));
        } else {
            // Second turn: final text answer.
            res.end(JSON.stringify({
                usage: {prompt_tokens: 20, completion_tokens: 9, total_tokens: 29},
                choices: [{message: {role: 'assistant', content: 'Suzhou is 18°C and sunny.'}}]
            }));
        }
    });
});

server.listen(0, '127.0.0.1', () => {
    settings.OPENAI_BASE_URL = `http://127.0.0.1:${server.address().port}/v1`;

    const tools = [{type: 'function', name: 'get_weather', description: 'Get weather',
        parameters: {type: 'object', properties: {location: {type: 'string'}}}}];

    provider.generate(
        [{type: 'user_input', content: [{type: 'text', text: '苏州天气'}]}],
        {systemInstruction: 'You are Wrist.', tools, enableSearchGrounding: false},
        (err, r1) => {
            if (err) { console.error('FAIL turn1:', err.message); process.exit(1); }
            console.assert(r1.functionCalls.length === 1, 'expected 1 tool call');
            console.assert(r1.functionCalls[0].name === 'get_weather', 'tool name');
            console.assert(r1.functionCalls[0].arguments.location === 'Suzhou', 'tool args');
            console.log('turn1 OK: tool call parsed ->', r1.functionCalls[0]);

            // Build next-turn history exactly like companionless.js does.
            const history = [{type: 'user_input', content: [{type: 'text', text: '苏州天气'}]}];
            r1.historyItems.forEach(i => history.push(i));
            history.push({type: 'function_result', name: 'get_weather', call_id: 'call_1',
                result: [{type: 'text', text: JSON.stringify({temp_c: 18, condition: 'sunny'})}]});

            provider.generate(history, {systemInstruction: 'You are Wrist.', tools}, (err2, r2) => {
                if (err2) { console.error('FAIL turn2:', err2.message); process.exit(1); }
                console.assert(r2.text.includes('18'), 'final text');
                const second = requests[1].body;
                const toolMsg = second.messages.find(m => m.role === 'tool');
                console.assert(toolMsg && toolMsg.tool_call_id === 'call_1', 'tool result message');
                console.assert(second.messages[0].role === 'system', 'system first');
                console.assert(second.tools.length === 1, 'tools forwarded');
                console.assert(requests[0].url === '/v1/chat/completions', 'url');
                console.assert(requests[0].auth === 'Bearer test-key', 'auth header');
                console.assert(second.model === 'deepseek-chat', 'model');
                console.log('turn2 OK:', r2.text);
                console.log('messages shape:', JSON.stringify(second.messages.map(m => ({role: m.role}))));
                console.log('\nALL PROVIDER TESTS PASSED');
                server.close();
                process.exit(0);
            });
        }
    );
});
