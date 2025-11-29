# Roadmap for Unreal Engine MCP Server

This roadmap outlines the development plan for expanding the capabilities of the Unreal Engine Model Context Protocol (MCP) server.

## Phase 1: Architecture & Foundation (✅ Completed)

- [x] **Native C++ Bridge**: Replace Python-based bridge with native C++ WebSocket plugin.
- [x] **Consolidated Tools**: Unify disparate tools into cohesive domains (`manage_asset`, `control_actor`, etc.).
- [x] **Modular Server**: Refactor monolithic `index.ts` into specialized subsystems (`ServerSetup`, `HealthMonitor`, `ResourceHandler`).
- [x] **WASM Acceleration**: Integrate Rust/WASM for high-performance JSON parsing and math.
- [x] **Robust Error Handling**: Standardize error responses, codes, and logging.
- [x] **Offline Capabilities**: Implement file-based fallback for project settings (`DefaultEngine.ini`).

## Phase 2: Graph & Logic Automation (✅ Completed)

- [x] **Blueprint Graphs**: Add/remove nodes, pins, and properties (`manage_blueprint_graph`).
- [x] **Material Graphs**: Edit material expressions and connections (`manage_material_graph`).
- [x] **Niagara Graphs**: Edit emitters, modules, and parameters (`manage_niagara_graph`).
- [x] **Behavior Trees**: Edit AI behavior trees, tasks, and decorators (`manage_behavior_tree`).
- [x] **Environment**: Unified environment builder (`build_environment`) for landscape, foliage, and proc-gen.

## Phase 3: Cinematic & Visual Automation (✅ Completed)

- [x] **Sequencer**: Full control over Level Sequences (create, play, tracks, keys, bindings).
- [x] **Audio**: Create SoundCues, play sounds, set mixes (`create_sound_cue`, `play_sound_at_location`).
- [x] **Landscape**: Sculpting, painting layers, modifying heightmaps (`sculpt_landscape`, `paint_landscape_layer`).
- [x] **Foliage**: Painting foliage instances and procedural spawning (`paint_foliage`).

## Phase 4: System & Developer Experience (Current)

- [x] **GraphQL API**: Read/write access to assets and actors via GraphQL.
- [x] **Pipeline Integration**: Direct UBT execution with output streaming.
- [x] **Documentation**: Comprehensive handler mappings and API references.
- [ ] **Real-time Streaming**: Streaming logs and test results via SSE or chunked responses.
- [ ] **Advanced Rendering**: Nanite/Lumen specific tools (partially implemented).
- [x] **Metrics Dashboard**: `ue://health` view backed by bridge/server metrics.

## Phase 5: Future Horizons

- [ ] **WASM Expansion**: Move more tools (for example, graph handlers) into Rust/WASM for performance and safety.
- [x] **GraphQL Leverage**: Let tools query via GraphQL (for example, asset search) and explore schema stitching for composite APIs.
- [x] **Monitoring & Metrics**: Prometheus-compatible endpoint for tool latency/errors and bridge metrics (pending requests, queue depth).
- [ ] **Extensibility Framework**: Dynamic handler registry via JSON config and support for custom C++ handlers via plugins.
- [ ] **Remote Profiling**: Deep integration with Unreal Insights for remote performance tuning.
- [ ] **AI-Driven Optimization**: Automated asset auditing and optimization suggestions.
- [ ] **Packaged Build Control**: Limited control over packaged executables (via sidecar or IPC).

## Legend

- ✅ **Completed**: Feature is implemented and verified.
- 🚧 **In Progress**: Feature is under active development.
- 📅 **Planned**: Feature is scheduled for a future release.