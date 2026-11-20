# Mica Privacy Policy

Last updated: 2026-11-20

Mica is a Pebble AI assistant designed around user-configured providers. It
does not operate a Mica-hosted AI, analytics, account, or subscription backend.

## Data stored on the phone

The Pebble phone app stores Mica settings locally, including API keys, model
and endpoint settings, optional profile notes, usage estimates, and local
conversation state. API keys are used to authenticate requests to the provider
selected by the user. Do not share a phone backup or settings export containing
credentials.

## AI requests

When the user submits a prompt, Mica sends the prompt and relevant conversation
or tool context from the phone to the selected AI provider or custom endpoint.
Depending on the enabled features, this can include profile notes and location
context. The provider's own terms and privacy policy apply. Mica supports
Google Gemini, Anthropic Claude, and OpenAI-compatible endpoints, including
user-selected local servers.

The optional Android companion, if separately installed and selected, processes
requests through that companion. Its additional permissions and third-party
account access require separate review and consent; the Mica PBW does not itself
grant Google account access.

## Location, weather, and maps

Location use is optional and can be disabled in Mica settings. When a feature
needs it, Mica may send:

- coordinates to Open-Meteo for weather;
- a place query or coordinates to Nominatim / OpenStreetMap-related services;
- coordinates to `staticmap.openstreetmap.de` to render a requested map; and
- location context to the selected AI provider when relevant to the request.

These recipients receive the IP address and technical request metadata visible
to their servers. Public services have their own logs and retention policies.

## Reminders and notifications

Mica uses the Pebble timeline and notification facilities to create reminders
requested by the user. Those facilities are provided by the installed Pebble
mobile/service stack and are subject to its policies.

## Feedback

Feedback and conversation reports are not sent to a Mica server by default. If
the user configures a feedback POST endpoint, submitted content and device
metadata are sent to that endpoint. If a GitHub issues URL is configured, Mica
can open a prefilled issue; the user reviews and submits it through GitHub.
Feedback is not automatically sent to the original Bobby or Billy authors.

## Network permissions

Mica requests Internet access for user-selected AI providers and related
features, local-network access for services such as Ollama or LM Studio, and
optional location, timeline, notification, and Android companion permissions.
A custom endpoint is controlled by the user; Mica cannot guarantee its privacy
or security practices. Plain-HTTP local endpoints should only be used on a
trusted network.

## Changes

Any privacy-impacting feature change should update this policy and the in-app
legal/location notices before release.
