# Contributing to Unreal Engine MCP Server

Thank you for your interest in contributing! This document provides guidelines and instructions for contributing.

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Pull Request Process](#pull-request-process)
- [Coding Standards](#coding-standards)

## Code of Conduct

Please be respectful and constructive in all interactions. We're building something together.

## Getting Started

### Prerequisites

- **Node.js** 18+ 
- **Unreal Engine** 5.0-5.7 (for live testing)
- **Git**

### Setup

1. Fork and clone the repository:
   ```bash
   git clone https://github.com/ChiR24/Unreal_mcp.git
   cd unreal-engine-mcp-server
   ```

2. Install dependencies:
   ```bash
   npm install
   ```

3. Build the project:
   ```bash
   npm run build
   ```

4. Run smoke tests (no Unreal needed):
   ```bash
   npm run test:smoke
   ```

## Development Workflow

### Branch Naming

Use descriptive branch names:
- `feat/add-new-tool` - New features
- `fix/connection-timeout` - Bug fixes
- `docs/update-readme` - Documentation
- `refactor/cleanup-bridge` - Code refactoring

### Running Tests

```bash
# Smoke test (CI-compatible, no Unreal needed)
npm run test:smoke

# Live tests (requires Unreal Editor running with plugin)
npm run test:all
```

### Building

```bash
npm run build        # Full build (TypeScript)
npm run build:core   # TypeScript only
```

## Pull Request Process

### Before Submitting

1. ✅ Run `npm run lint` and fix any issues
2. ✅ Run `npm run test:smoke` to verify basic functionality
3. ✅ Update documentation if adding new features
4. ✅ Update `CHANGELOG.md` under `[Unreleased]`

### PR Title Format

We use [Conventional Commits](https://www.conventionalcommits.org/). PR titles must follow this format:

```
<type>: <description>

Examples:
feat: add new animation blending tool
fix: resolve WebSocket connection timeout
docs: update installation instructions  
refactor: simplify blueprint graph handler
```

**Types:** `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`, `revert`

### Review Process

1. All PRs require at least one approval
2. CI must pass (lint, smoke test, CodeQL)
3. Maintainers may request changes

## Coding Standards

### TypeScript

- Use strict TypeScript (`strict: true`)
- Prefer `async/await` over raw Promises
- Use descriptive variable names
- Add JSDoc comments for public APIs

### C++ (Plugin)

- Follow Unreal Engine coding standards
- Use `UPROPERTY`/`UFUNCTION` macros appropriately
- Keep thread safety in mind for async operations

### File Organization

```
src/
├── tools/           # MCP tool implementations
├── automation/      # Unreal bridge logic
├── utils/           # Shared utilities
└── services/        # Background services

Plugins/
└── McpAutomationBridge/  # C++ Unreal plugin
```

## Questions?

- Open a [Discussion](../../discussions) for questions
- Open an [Issue](../../issues) for bugs or feature requests

Thank you for contributing! 🎮
