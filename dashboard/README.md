# Dashboard README

The EtherVoxAI dashboard is a Vue 3 + Vite application that surfaces runtime metrics, audio status, and configuration controls exposed through the core REST API. This document explains how to install dependencies, run the local development server, and contribute updates.

## Installation

- Prerequisites: Node.js 18.x and npm 9.x (match the versions used in CI).
- Install dependencies from the repository root once with `npm install`, then install dashboard packages with `npm install` from this directory.
- If you are working on the dashboard in isolation, run `npm ci` to get a clean dependency tree consistent with lockfile expectations.

## Usage

- Start the development server with `npm run dev`; the app runs on `http://localhost:5173` by default and proxies API calls to the backend at `http://localhost:3000` unless overridden in `vite.config.js`.
- Build production assets with `npm run build`; the compiled output lands under `dist/` and is consumed by the root build when `npm run build` executes from the repository root.
- Run formatting and lint checks with `npm run lint`; add `-- --fix` to automatically resolve common issues flagged by ESLint.

## Project structure

- `src/main.js` bootstraps Vue, registers the router, and wires Tailwind.
- `src/router/index.js` defines route layouts and guards for authenticated views.
- `src/stores/system.js` (Pinia) caches metrics returned from `/api/system/status` and `/api/system/metrics`.
- `src/layouts/` holds shared shells that wrap routes; `src/components/` contains reusable UI widgets such as charts and status panels.

## Contributing

- Follow the repository-wide guidance in `CONTRIBUTING.md`, especially the sections on localization and dashboard testing.
- Add or update Cypress component tests in `tests/` whenever you introduce new interactive behavior; mock API responses to keep tests deterministic.
- Update `docs/mvp.md` and `dashboard/tailwind.config.js` if you add new design primitives that other contributors should reuse.

## License

All dashboard files are covered by the repository license (`LICENSE` at the root). Include the SPDX header (`SPDX-License-Identifier: CC-BY-NC-SA-4.0`) in any new source files to maintain compliance.