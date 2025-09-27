# EthervoxAI MVP Overview

The EthervoxAI MVP is a privacy-first, multilingual voice intelligence platform designed to run natively on edge hardware. This document outlines the initial product scope, guiding principles, and feature priorities for the first functional release.

---

## MVP Vision

Enable seamless, ambient, multilingual voice interaction across smart devices—while preserving user privacy, maintaining local-first execution, and empowering end users with full control over their data.

---

## Core Principles

- **Local-First Execution**: All speech processing, intent detection, and language modeling performed on-device.
- **Multilingual Seamlessness**: Auto-switch spoken and response language based on user input; no manual toggling required.
- **Ambient Interaction**: No wake words required. Devices operate seamlessly in context, optionally personalized with local names.
- **User Data Sovereignty**: Transparent local UI + dashboard showing data flows, history, and user-approved cloud queries.
- **Extensibility**: Modular SDK and plugin system for LLM routing and hardware integrations.

---

## MVP Feature Set

| Feature                     | Description                                                           |
|----------------------------|-----------------------------------------------------------------------|
| Speech Runtime          | Local STT + TTS with real-time language identification                |
| Dialogue Engine          | Lightweight intent parser + embedded LLM + pluggable external LLMs    |
| Privacy Dashboard (UI)   | Web + mobile interface for configuration, data transparency            |
| External LLM Plugin API  | Optional integration with OpenAI, HuggingFace models, or local RAG    |
| Hardware Targets         | ESP32-S3 and Raspberry Pi 5 Dev Kits with mic-array support           |
| Developer SDK            | Add-on modules for new intents, model routing, diagnostics            |

---

## 🛠️ Technical Stack Snapshot

- **Local Models**: GGUF + Whisper Tiny or Mistral Lite (optimized for embedded use)
- **Language Switching**: Profile-based with ambient voice detection and fallback
- **UI Framework**: Vue.js for dashboard, Tailwind for styling
- **Hardware I/O**: ALSA or PulseAudio drivers, TensorRT (RPi) support

---

## 🧪 Initial Supported Languages

- **English (US/UK)**
- **Spanish (LatAm)**
- **Mandarin (Simplified)**  
_(Scalable to >15 languages post-MVP)_

---

## 🛤️ MVP Milestones

1. 🧱 Local runtime + STT/TTS integration
2. 🧠 Intent parser and basic LLM container
3. 🔐 Privacy dashboard with audit-ready cloud query logging
4. 🔌 External LLM integration framework
5. 🧪 Prototype voice activation on Raspberry Pi
6. 🎛️ Developer SDK first release (intent plugins + device profiles)

---

## 📝 Notes & Assumptions

- Cloud access only via opt-in and only for web/RAG queries
- MVP includes sandbox examples for kitchen appliance and robot use cases
- All data is processed locally unless explicitly triggered by user query

---

## 🧑‍🤝‍🧑 Want to Join?

Start by reading [`CONTRIBUTING.md`](../CONTRIBUTING.md) and [`ethical-ai.md`](./ethical-ai.md). We welcome collaborators who share our values and want to help shape ambient intelligence with integrity.

Have questions or ideas? Reach us at:  
📧 team@ethervox.ai

