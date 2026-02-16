# wasm

Rust-based performance layer for math and property parsing.

## OVERVIEW
High-performance Rust modules compiled to WebAssembly with TypeScript fallbacks. 5-10x speedup for math operations when enabled.

## STRUCTURE
```
wasm/
├── src/
│   ├── lib.rs              # WASM exports
│   ├── property_parser.rs  # UE property string parsing
│   ├── transform_math.rs   # Matrix/Vector math
│   └── dependency_resolver.rs  # Asset dependency graphs
├── Cargo.toml              # Rust crate config
└── pkg/                    # Compiled output (wasm-pack)
```

## CONVENTIONS
- **Safety**: No `unsafe` blocks unless absolutely required for FFI.
- **Fallbacks**: Every WASM function must have a matching TS implementation in `src/wasm/index.ts`.
- **Types**: Use `wasm-bindgen` for type generation.
- **f32**: All floating-point operations use f32 for performance.

## ANTI-PATTERNS
- **Heavy FFI**: Avoid frequent small calls between TS and Rust; batch data instead.
- **Large Binaries**: Minimize dependency tree to keep `.wasm` size under 500KB.
