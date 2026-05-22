# EthervoxAI macOS Native App - Implementation Guide

**Target**: Production-ready macOS application wrapping the EthervoxAI C core
**Architecture**: Swift/SwiftUI UI + Existing C Library
**Timeline**: 2-4 weeks for full implementation

---

## Project Overview

Build a polished native macOS application using Swift and SwiftUI that wraps the existing EthervoxAI C library. This approach preserves the cross-platform C core while delivering a native macOS experience with:

- Modern SwiftUI interface
- Token streaming with live updates
- Tool execution visualization
- Memory/conversation management
- Model download and configuration
- App Store distribution ready

---

## Directory Structure

```
EthervoxAI-macOS/
├── EthervoxAI.xcodeproj
├── EthervoxAI/                          # Main app target
│   ├── App/
│   │   ├── EthervoxAIApp.swift         # @main entry point
│   │   └── AppDelegate.swift
│   ├── Views/
│   │   ├── ContentView.swift           # Main window
│   │   ├── ConversationView.swift      # Chat UI
│   │   ├── SettingsView.swift
│   │   └── Components/
│   │       ├── MessageBubble.swift
│   │       ├── ThinkingIndicator.swift
│   │       └── ToolCallView.swift
│   ├── ViewModels/                      # MVVM pattern
│   │   ├── ConversationViewModel.swift
│   │   ├── GovernorViewModel.swift
│   │   └── SettingsViewModel.swift
│   ├── Services/
│   │   ├── EthervoxEngine.swift        # Main C bridge
│   │   ├── ModelManager.swift
│   │   └── MemoryService.swift
│   └── Resources/
│       └── Assets.xcassets
│
├── EthervoxCore/                        # Framework target
│   ├── Bridging/
│   │   ├── module.modulemap
│   │   └── EthervoxCore.h
│   ├── CLibrary/
│   │   ├── include/                     # From cpp/include/ethervox/
│   │   └── external/                    # llama.cpp
│   ├── Prebuilt/
│   │   ├── libethervoxai.a
│   │   └── libllama.a
│   └── Swift/
│       ├── Governor.swift
│       ├── MemoryStore.swift
│       └── ComputeTools.swift
│
└── Models/                              # Downloaded at runtime
    └── .gitkeep
```

---

## Week 1: C Library Integration

### Day 1-2: Project Setup

**Tasks:**
1. Create Xcode project with macOS App template
2. Add EthervoxCore framework target
3. Set up git submodule for llama.cpp
4. Build C library for macOS (`cmake -DTARGET_PLATFORM=MACOS`)

**Build Commands:**
```bash
# Build C library
cd /path/to/ethervoxai-android/ethervox_multiplatform_core/src/main/cpp
cmake -DTARGET_PLATFORM=MACOS -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build

# Copy artifacts to Xcode project
cp build/libethervoxai.a ../EthervoxAI-macOS/EthervoxCore/Prebuilt/
cp -r include/ethervox ../EthervoxAI-macOS/EthervoxCore/CLibrary/include/
```

**module.modulemap:**
```c
module EthervoxCore {
    umbrella header "EthervoxCore.h"
    export *
    module * { export * }
    link "ethervoxai"
    link "llama"
}
```

**EthervoxCore.h (Umbrella Header):**
```c
#ifndef ETHERVOXCORE_H
#define ETHERVOXCORE_H

#include "governor.h"
#include "memory_tools.h"
#include "compute_tools.h"
#include "file_tools.h"
#include "model_manager.h"
#include "platform.h"
#include "config.h"

#endif
```

### Day 3-4: Basic Swift Wrapper

**Governor.swift** - Core C bridge:
```swift
import Foundation
import EthervoxCore

class Governor {
    private var handle: OpaquePointer?
    private var registry: OpaquePointer?
    private var progressCallback: ((GovernorEvent, String) -> Void)?
    private var tokenCallback: ((String) -> Void)?
    
    // C callback trampolines
    private static let cProgressCallback: ethervox_governor_progress_callback = { 
        eventType, message, userData in
        guard let userData = userData else { return }
        let governor = Unmanaged<Governor>.fromOpaque(userData).takeUnretainedValue()
        
        let event = GovernorEvent(rawValue: eventType.rawValue)!
        let msg = String(cString: message)
        
        DispatchQueue.main.async {
            governor.progressCallback?(event, msg)
        }
    }
    
    private static let cTokenCallback: (@convention(c) (UnsafePointer<CChar>?, UnsafeMutableRawPointer?) -> Void) = { 
        token, userData in
        guard let token = token, let userData = userData else { return }
        let governor = Unmanaged<Governor>.fromOpaque(userData).takeUnretainedValue()
        
        let tokenStr = String(cString: token)
        DispatchQueue.main.async {
            governor.tokenCallback?(tokenStr)
        }
    }
    
    init() throws {
        // Initialize registry
        var reg = ethervox_tool_registry_t()
        guard ethervox_tool_registry_init(&reg, 16) == 0 else {
            throw GovernorError.registryInitFailed
        }
        
        // Register tools
        ethervox_compute_tools_register_all(&reg)
        ethervox_memory_tools_register_all(&reg)
        ethervox_file_tools_register_all(&reg)
        
        registry = withUnsafeMutablePointer(to: &reg) { OpaquePointer($0) }
        
        // Initialize governor
        var config = ethervox_governor_default_config()
        var gov: OpaquePointer?
        guard ethervox_governor_init(&gov, &config, registry) == 0 else {
            throw GovernorError.initFailed
        }
        handle = gov
    }
    
    func loadModel(path: String) throws {
        guard let handle = handle else { throw GovernorError.notInitialized }
        
        let result = path.withCString { cPath in
            ethervox_governor_load_model(handle, cPath)
        }
        
        guard result == 0 else {
            throw GovernorError.modelLoadFailed
        }
    }
    
    func execute(
        query: String,
        onProgress: @escaping (GovernorEvent, String) -> Void,
        onToken: @escaping (String) -> Void
    ) async throws -> String {
        guard let handle = handle else { throw GovernorError.notInitialized }
        
        self.progressCallback = onProgress
        self.tokenCallback = onToken
        
        return try await withCheckedThrowingContinuation { continuation in
            var response: UnsafeMutablePointer<CChar>?
            var error: UnsafeMutablePointer<CChar>?
            var metrics = ethervox_confidence_metrics_t()
            
            let userData = Unmanaged.passUnretained(self).toOpaque()
            
            defer {
                if let ptr = response { free(ptr) }
                if let ptr = error { free(ptr) }
            }
            
            let status = query.withCString { cQuery in
                ethervox_governor_execute(
                    handle,
                    cQuery,
                    &response,
                    &error,
                    &metrics,
                    Governor.cProgressCallback,
                    Governor.cTokenCallback,
                    userData
                )
            }
            
            if status == ETHERVOX_GOVERNOR_SUCCESS, let response = response {
                let result = String(cString: response)
                continuation.resume(returning: result)
            } else {
                let errorMsg = error.flatMap { String(cString: $0) } ?? "Unknown error"
                continuation.resume(throwing: GovernorError.executionFailed(errorMsg))
            }
        }
    }
    
    deinit {
        if let handle = handle {
            ethervox_governor_cleanup(handle)
        }
        if let reg = registry {
            var regStruct = reg.assumingMemoryBound(to: ethervox_tool_registry_t.self).pointee
            ethervox_tool_registry_cleanup(&regStruct)
        }
    }
}

enum GovernorError: Error {
    case registryInitFailed
    case initFailed
    case notInitialized
    case modelLoadFailed
    case executionFailed(String)
}

enum GovernorEvent: Int {
    case thinking = 0
    case toolCall = 1
    case toolResult = 2
    case complete = 3
    case error = 4
}
```

### Day 5: Build Configuration

**Xcode Build Settings:**

1. **Other Linker Flags:**
   ```
   -lethervoxai
   -lllama
   -framework CoreAudio
   -framework AudioUnit
   -framework Metal
   -framework MetalKit
   ```

2. **Library Search Paths:**
   ```
   $(PROJECT_DIR)/EthervoxCore/Prebuilt
   ```

3. **Header Search Paths:**
   ```
   $(PROJECT_DIR)/EthervoxCore/CLibrary/include
   ```

4. **Metal Compiler:**
   - Enable Metal compilation
   - Copy `ggml-metal.metal` to Resources

---

## Week 2: Core Features

### Day 6-7: MVVM Architecture

**ConversationViewModel.swift:**
```swift
@MainActor
class ConversationViewModel: ObservableObject {
    @Published var messages: [Message] = []
    @Published var isProcessing = false
    @Published var currentThought: String = ""
    @Published var toolCalls: [ToolCall] = []
    
    private let engine: EthervoxEngine
    
    init(engine: EthervoxEngine) {
        self.engine = engine
    }
    
    func send(_ text: String) async {
        isProcessing = true
        toolCalls.removeAll()
        currentThought = ""
        
        defer { isProcessing = false }
        
        let userMessage = Message(role: .user, content: text)
        messages.append(userMessage)
        
        do {
            let response = try await engine.processQuery(
                text,
                onProgress: { [weak self] event, message in
                    await self?.handleProgress(event, message: message)
                },
                onToken: { [weak self] token in
                    await self?.handleToken(token)
                }
            )
            
            messages.append(Message(role: .assistant, content: response))
        } catch {
            messages.append(Message(role: .system, content: "Error: \(error.localizedDescription)"))
        }
    }
    
    private func handleProgress(_ event: GovernorEvent, message: String) async {
        switch event {
        case .thinking:
            currentThought = message
        case .toolCall:
            toolCalls.append(ToolCall(name: message, status: .running))
        case .toolResult:
            if !toolCalls.isEmpty {
                toolCalls[toolCalls.count - 1].status = .completed
            }
        default:
            break
        }
    }
    
    private func handleToken(_ token: String) async {
        // Append to last assistant message for streaming effect
        if let last = messages.last, last.role == .assistant {
            messages[messages.count - 1].content += token
        } else {
            messages.append(Message(role: .assistant, content: token))
        }
    }
}

struct Message: Identifiable {
    let id = UUID()
    let role: Role
    var content: String
    
    enum Role {
        case user, assistant, system
    }
}

struct ToolCall: Identifiable {
    let id = UUID()
    var name: String
    var status: Status
    
    enum Status {
        case running, completed, failed
    }
}
```

### Day 8-9: SwiftUI Views

**ConversationView.swift:**
```swift
struct ConversationView: View {
    @StateObject private var viewModel: ConversationViewModel
    @State private var inputText = ""
    
    var body: some View {
        VStack(spacing: 0) {
            // Messages
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(spacing: 12) {
                        ForEach(viewModel.messages) { message in
                            MessageBubble(message: message)
                                .id(message.id)
                        }
                        
                        if viewModel.isProcessing {
                            ThinkingIndicator(thought: viewModel.currentThought)
                            
                            ForEach(viewModel.toolCalls) { toolCall in
                                ToolCallView(toolCall: toolCall)
                            }
                        }
                    }
                    .padding()
                }
                .onChange(of: viewModel.messages.count) { _ in
                    if let last = viewModel.messages.last {
                        withAnimation {
                            proxy.scrollTo(last.id, anchor: .bottom)
                        }
                    }
                }
            }
            
            Divider()
            
            // Input
            HStack(spacing: 12) {
                TextField("Ask anything...", text: $inputText)
                    .textFieldStyle(.roundedBorder)
                    .onSubmit { sendMessage() }
                
                Button(action: sendMessage) {
                    Image(systemName: "paperplane.fill")
                        .foregroundColor(.white)
                }
                .buttonStyle(.borderedProminent)
                .disabled(viewModel.isProcessing || inputText.isEmpty)
            }
            .padding()
        }
    }
    
    private func sendMessage() {
        let text = inputText
        inputText = ""
        
        Task {
            await viewModel.send(text)
        }
    }
}
```

**MessageBubble.swift:**
```swift
struct MessageBubble: View {
    let message: Message
    
    var body: some View {
        HStack {
            if message.role == .user { Spacer() }
            
            VStack(alignment: message.role == .user ? .trailing : .leading, spacing: 4) {
                Text(message.content)
                    .padding(12)
                    .background(backgroundColor)
                    .foregroundColor(textColor)
                    .cornerRadius(16)
                    .textSelection(.enabled)
                
                Text(timeString)
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
            
            if message.role == .assistant { Spacer() }
        }
    }
    
    private var backgroundColor: Color {
        switch message.role {
        case .user: return .blue
        case .assistant: return Color(.systemGray5)
        case .system: return .orange
        }
    }
    
    private var textColor: Color {
        message.role == .user ? .white : .primary
    }
    
    private var timeString: String {
        let formatter = DateFormatter()
        formatter.timeStyle = .short
        return formatter.string(from: Date())
    }
}
```

### Day 10: Model Management

**ModelManager.swift:**
```swift
class ModelManager: ObservableObject {
    @Published var downloadProgress: Double = 0
    @Published var isDownloading = false
    @Published var availableModels: [ModelInfo] = []
    
    private let modelsDirectory: URL
    
    init() {
        modelsDirectory = FileManager.default.urls(
            for: .applicationSupportDirectory,
            in: .userDomainMask
        )[0].appendingPathComponent("EthervoxAI/models")
        
        try? FileManager.default.createDirectory(
            at: modelsDirectory,
            withIntermediateDirectories: true
        )
        
        loadAvailableModels()
    }
    
    func ensureModelAvailable(modelName: String = "Qwen2.5-3B-Instruct-Q4_K_M.gguf") async throws -> URL {
        let modelPath = modelsDirectory.appendingPathComponent(modelName)
        
        if FileManager.default.fileExists(atPath: modelPath.path) {
            return modelPath
        }
        
        // Download model
        try await downloadModel(modelName)
        return modelPath
    }
    
    private func downloadModel(_ modelName: String) async throws {
        isDownloading = true
        defer { isDownloading = false }
        
        // Use C library's model manager
        var manager: OpaquePointer?
        var config = ethervox_model_manager_get_default_config()
        
        modelsDirectory.path.withCString { path in
            config.models_dir = path
        }
        
        manager = ethervox_model_manager_create(&config)
        defer { 
            if let mgr = manager {
                ethervox_model_manager_destroy(mgr)
            }
        }
        
        // Download (blocks - run in Task)
        let result = ethervox_model_manager_download(manager, &ETHERVOX_MODEL_QWEN_3B)
        
        guard result == 0 else {
            throw ModelError.downloadFailed
        }
    }
    
    private func loadAvailableModels() {
        // Scan models directory
        guard let contents = try? FileManager.default.contentsOfDirectory(
            at: modelsDirectory,
            includingPropertiesForKeys: [.fileSizeKey]
        ) else { return }
        
        availableModels = contents
            .filter { $0.pathExtension == "gguf" }
            .compactMap { url in
                guard let size = try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize else {
                    return nil
                }
                return ModelInfo(name: url.lastPathComponent, size: size, path: url)
            }
    }
}

struct ModelInfo: Identifiable {
    let id = UUID()
    let name: String
    let size: Int
    let path: URL
    
    var sizeString: String {
        ByteCountFormatter.string(fromByteCount: Int64(size), countStyle: .file)
    }
}

enum ModelError: Error {
    case downloadFailed
    case notFound
}
```

---

## Week 3: Polish & Features

### Day 11-12: Settings & Preferences

**SettingsView.swift:**
```swift
struct SettingsView: View {
    @AppStorage("temperature") private var temperature = 0.7
    @AppStorage("maxTokens") private var maxTokens = 2048
    @AppStorage("selectedModel") private var selectedModel = ""
    
    @StateObject private var modelManager = ModelManager()
    
    var body: some View {
        Form {
            Section("Model") {
                Picker("Active Model", selection: $selectedModel) {
                    ForEach(modelManager.availableModels) { model in
                        Text("\(model.name) (\(model.sizeString))")
                            .tag(model.path.path)
                    }
                }
                
                if modelManager.isDownloading {
                    ProgressView(value: modelManager.downloadProgress)
                        .progressViewStyle(.linear)
                }
                
                Button("Download More Models") {
                    // Show model download sheet
                }
            }
            
            Section("Generation") {
                Slider(value: $temperature, in: 0...1) {
                    Text("Temperature: \(temperature, specifier: "%.2f")")
                }
                
                Stepper("Max Tokens: \(maxTokens)", value: $maxTokens, in: 128...4096, step: 128)
            }
            
            Section("File Tools") {
                Button("Add Allowed Directory") {
                    addAllowedDirectory()
                }
                
                // List allowed directories
            }
            
            Section("About") {
                LabeledContent("Version", value: Bundle.main.version)
                LabeledContent("Model License", value: "Apache 2.0 (Qwen2.5)")
                
                Button("View Licenses") {
                    // Show license sheet
                }
            }
        }
        .formStyle(.grouped)
        .frame(width: 500, height: 400)
    }
    
    private func addAllowedDirectory() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canCreateDirectories = false
        
        panel.begin { response in
            guard response == .OK, let url = panel.url else { return }
            
            // Create security-scoped bookmark
            do {
                let bookmark = try url.bookmarkData(
                    options: .withSecurityScope,
                    includingResourceValuesForKeys: nil,
                    relativeTo: nil
                )
                // Save bookmark
                UserDefaults.standard.set(bookmark, forKey: "allowedDirs")
            } catch {
                print("Failed to create bookmark: \(error)")
            }
        }
    }
}

extension Bundle {
    var version: String {
        "\(infoDictionary?["CFBundleShortVersionString"] as? String ?? "1.0") (\(infoDictionary?["CFBundleVersion"] as? String ?? "1"))"
    }
}
```

### Day 13-14: Memory & Conversation History

**MemoryService.swift:**
```swift
class MemoryService: ObservableObject {
    @Published var memories: [MemoryEntry] = []
    @Published var reminders: [Reminder] = []
    
    private var store: OpaquePointer?
    
    init() {
        // Initialize memory store
        var memStore = ethervox_memory_store_t()
        
        let dataDir = FileManager.default.urls(
            for: .applicationSupportDirectory,
            in: .userDomainMask
        )[0].appendingPathComponent("EthervoxAI/memory")
        
        try? FileManager.default.createDirectory(at: dataDir, withIntermediateDirectories: true)
        
        dataDir.path.withCString { path in
            ethervox_memory_init(&memStore, "default", path)
        }
        
        store = withUnsafeMutablePointer(to: &memStore) { OpaquePointer($0) }
        
        loadMemories()
    }
    
    func search(query: String) -> [MemoryEntry] {
        guard let store = store else { return [] }
        
        var results: UnsafeMutablePointer<ethervox_memory_entry_t>?
        var count: UInt32 = 0
        
        query.withCString { cQuery in
            let storePtr = store.assumingMemoryBound(to: ethervox_memory_store_t.self)
            ethervox_memory_search(storePtr, cQuery, nil, 0, 0.0, 10, &results, &count)
        }
        
        defer {
            if let results = results {
                free(results)
            }
        }
        
        guard let results = results else { return [] }
        
        return (0..<Int(count)).map { i in
            let entry = results[i]
            return MemoryEntry(
                id: entry.memory_id,
                text: String(cString: entry.text),
                importance: entry.importance,
                timestamp: Date(timeIntervalSince1970: TimeInterval(entry.timestamp))
            )
        }
    }
    
    private func loadMemories() {
        // Load recent memories
        memories = search(query: "")
    }
    
    deinit {
        if let store = store {
            var storeStruct = store.assumingMemoryBound(to: ethervox_memory_store_t.self).pointee
            ethervox_memory_cleanup(&storeStruct)
        }
    }
}

struct MemoryEntry: Identifiable {
    let id: UInt64
    let text: String
    let importance: Float
    let timestamp: Date
}

struct Reminder: Identifiable {
    let id = UUID()
    let text: String
    let deadline: Date?
    let completed: Bool
}
```

### Day 15: Sandboxing & Entitlements

**EthervoxAI.entitlements:**
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.app-sandbox</key>
    <true/>
    
    <!-- File access for file tools -->
    <key>com.apple.security.files.user-selected.read-write</key>
    <true/>
    
    <!-- No network needed - 100% on-device -->
    <key>com.apple.security.network.client</key>
    <false/>
    
    <!-- Allow Metal GPU access -->
    <key>com.apple.security.device.audio-input</key>
    <false/>
</dict>
</plist>
```

---

## Week 4: Testing & Distribution

### Day 16-17: Testing

**Test Checklist:**
- [ ] Model loading and initialization
- [ ] Token streaming performance
- [ ] Tool execution (calculator, memory, file)
- [ ] Memory sandboxing (security-scoped bookmarks)
- [ ] Model download with progress
- [ ] Conversation history persistence
- [ ] Settings persistence
- [ ] Dark mode support
- [ ] Window state restoration
- [ ] Crash recovery

### Day 18-19: App Store Preparation

**Info.plist Additions:**
```xml
<key>NSHumanReadableCopyright</key>
<string>© 2025 EthervoxAI. Uses Apache 2.0 licensed models.</string>

<key>NSUsageDescription</key>
<string>EthervoxAI processes all data locally on your device. No internet connection required.</string>

<key>LSApplicationCategoryType</key>
<string>public.app-category.productivity</string>

<key>LSMinimumSystemVersion</key>
<string>14.0</string>
```

**App Store Metadata:**
- **Privacy Nutrition Label**: "Data Not Collected"
- **Category**: Productivity
- **Description**: Emphasize 100% on-device processing
- **Keywords**: AI assistant, local LLM, privacy, offline
- **Screenshots**: Show conversation UI, settings, tool usage

### Day 20: Code Signing & Notarization

```bash
# Sign framework
codesign --force --deep --sign "Apple Development" \
    EthervoxAI.app/Contents/Frameworks/EthervoxCore.framework

# Sign app
codesign --force --deep --sign "Apple Development" \
    --entitlements EthervoxAI.entitlements \
    EthervoxAI.app

# Create archive
xcodebuild archive \
    -scheme EthervoxAI \
    -configuration Release \
    -archivePath EthervoxAI.xcarchive

# Export for distribution
xcodebuild -exportArchive \
    -archivePath EthervoxAI.xcarchive \
    -exportPath Export \
    -exportOptionsPlist ExportOptions.plist

# Notarize
xcrun notarytool submit Export/EthervoxAI.app.zip \
    --apple-id your@email.com \
    --team-id TEAMID \
    --password app-specific-password
```

---

## Key Technical Patterns

### C to Swift Memory Management

**Rule 1: C-allocated strings must use C's `free()`**
```swift
var cString: UnsafeMutablePointer<CChar>?
defer {
    if let ptr = cString {
        free(ptr)  // Use C free, not Swift
    }
}
let swiftString = String(cString: cString!)
```

**Rule 2: Swift-to-C strings use `.withCString()`**
```swift
let swiftString = "Hello"
swiftString.withCString { cString in
    some_c_function(cString)
}
```

**Rule 3: Opaque pointers for C structs**
```swift
private var governor: OpaquePointer?  // ethervox_governor_t*

deinit {
    if let handle = governor {
        ethervox_governor_cleanup(handle)
    }
}
```

### Callback Pattern

**C Function Pointer → Swift Closure**
```swift
// Store Swift closure
private var callback: ((String) -> Void)?

// C trampoline
private static let cCallback: (@convention(c) (UnsafePointer<CChar>?, UnsafeMutableRawPointer?) -> Void) = { 
    data, userData in
    guard let data = data, let userData = userData else { return }
    
    // Extract Swift object from userData
    let obj = Unmanaged<MyClass>.fromOpaque(userData).takeUnretainedValue()
    
    // Call Swift closure on main thread
    DispatchQueue.main.async {
        obj.callback?(String(cString: data))
    }
}

// Pass to C
let userData = Unmanaged.passUnretained(self).toOpaque()
c_function_with_callback(MyClass.cCallback, userData)
```

### Actor-Based Concurrency

```swift
actor EthervoxEngine {
    private var governor: OpaquePointer?
    
    func processQuery(_ query: String) async throws -> String {
        // Runs on background thread automatically
        return try await withCheckedThrowingContinuation { continuation in
            // C library call here
        }
    }
}

@MainActor
class ViewModel: ObservableObject {
    private let engine = EthervoxEngine()
    
    func send(_ text: String) async {
        let result = try await engine.processQuery(text)
        // Back on main thread
        messages.append(result)
    }
}
```

---

## Distribution Options

### Option 1: App Store (Recommended)

**Pros:**
- Built-in update mechanism
- Discoverability
- Trust/credibility

**Cons:**
- 30% commission
- Review process (2-3 days)
- Model size restrictions (4GB limit)

**Solution for models:**
Download on first launch (not in bundle)

### Option 2: Direct Download

**Pros:**
- No commission
- Full control
- Can bundle models

**Cons:**
- Users need to bypass Gatekeeper
- Manual updates
- Less trust

**Requirements:**
- Developer ID signing
- Notarization required
- DMG or PKG installer

---

## Performance Targets

### Startup Time
- **Cold start**: < 3 seconds (model not loaded)
- **Model load**: 5-10 seconds (3B model)
- **First query**: < 2 seconds after model load

### Inference Speed
- **3B model (Q4)**: 20-40 tokens/sec on M1
- **Memory usage**: ~4GB RAM with model loaded
- **GPU utilization**: 60-80% during inference

### UI Responsiveness
- **Token streaming**: 60 FPS updates
- **Scroll performance**: No dropped frames
- **Settings changes**: Instant feedback

---

## Troubleshooting Guide

### Common Issues

**1. "No such module 'EthervoxCore'"**
- Verify module.modulemap exists
- Check Header Search Paths
- Ensure framework is linked

**2. "Undefined symbols" during linking**
- Add `-lethervoxai -lllama` to Other Linker Flags
- Verify Library Search Paths
- Check that .a files are in Prebuilt/

**3. Metal shaders not found**
- Copy ggml-metal.metal to Resources
- Add to Copy Bundle Resources build phase
- Verify Metal framework is linked

**4. Sandboxing blocks file access**
- Implement security-scoped bookmarks
- Add `com.apple.security.files.user-selected.read-write`
- Persist bookmarks in UserDefaults

**5. Model download fails**
- Check Application Support directory permissions
- Verify disk space (3GB+ needed)
- Test with smaller model first

---

## Resources

### Documentation
- [Swift/C Interop Guide](https://developer.apple.com/documentation/swift/using-imported-c-functions-in-swift)
- [App Sandbox Documentation](https://developer.apple.com/documentation/security/app_sandbox)
- [Metal Programming Guide](https://developer.apple.com/metal/)

### Code References
- `src/main.c` (lines 830-900): REPL execution pattern
- `include/ethervox/governor.h`: Complete API documentation
- `src/platform/platform_macos.c`: macOS-specific configuration
- `external/llama.cpp/ggml/src/ggml-metal.m`: Metal backend reference

### Tools
- **Xcode Instruments**: Profile memory and performance
- **lldb**: Debug C/Swift boundary issues
- **codesign**: Verify signing and entitlements
- **spctl**: Test Gatekeeper acceptance

---

## Next Steps

1. **Week 1**: Set up Xcode project, link C library, test basic Governor wrapper
2. **Week 2**: Implement MVVM, build conversation UI, add token streaming
3. **Week 3**: Polish UI, add settings, implement model download
4. **Week 4**: Test sandboxing, prepare App Store submission, notarize

**Estimated Total Time**: 2-4 weeks for polished, App Store-ready application

---

*Last Updated: December 3, 2025*
