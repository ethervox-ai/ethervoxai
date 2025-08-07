# 📦 EthervoxAI Modules Overview

Welcome to the EthervoxAI modular architecture documentation. This folder contains detailed specs for the three foundational modules powering the MVP:

---

## 🌐 Multilingual Runtime
**Purpose:** Enable seamless voice interaction across multiple languages without manual switching.

**Highlights:**
- Real-time language detection and switching
- Local STT/TTS engines for privacy and speed
- Language profiles per user or device

**Architecture:**
[Mic Input] → [STT Engine] → [Language Detector] → [Intent Parser] → [TTS Engine] → [Speaker Output]


📄 See [`multilingual-runtime.md`](./multilingual-runtime.md)

---

## 🧠 Local LLM Stack
**Purpose:** Provide on-device intelligence for parsing, generating, and routing responses with optional cloud augmentation.

**Highlights:**
- GGUF-based models (e.g., Mistral Lite, TinyLlama)
- Hybrid intent parsing (rules + ML)
- Plugin API for external LLMs with user consent

**Architecture:**
[Text Input] → [Intent Parser] → [Local LLM] → [Response Generator] ↘ (Optional) → [External LLM Plugin]


📄 See [`local-llm-stack.md`](./local-llm-stack.md)

---

## 🔐 Privacy Dashboard
**Purpose:** Empower users with full control over data flow, cloud access, and retention policies.

**Highlights:**
- Real-time cloud interaction logs
- Consent toggles per device or query
- Role-based access and encryption

**Architecture:**
[User Interface] → [Privacy Controller] → [Cloud Router] ↘ [Audit Log] → [Local Storage]


📄 See [`privacy-dashboard.md`](./privacy-dashboard.md)

---

## 🔭 Coming Soon
- `context-memory.md`: Session-aware memory and personalization
- `plugin-framework.md`: Modular plugin architecture for third-party integrations
- `voice-agent-orchestration.md`: Multi-agent coordination and fallback logic

---

For questions or contributions, visit the [EthervoxAI GitHub repo](https://github.com/ethervox-ai/ethervoxai).
