# wasm

Rust-based performance layer for math and property parsing.

## OVERVIEW
High-performance Rust modules compiled to WebAssembly. Provides 5-10x speedup for JSON parsing, transform math, and dependency resolution. Automatic TS fallback when WASM unavailable.

## STRUCTURE
```
wasm/
├── src/
│   ├── lib.rs                 # WASM exports (wasm-bindgen)
│   ├── property_parser.rs     # UE property string parsing
│   ├── transform_math.rs      # Matrix/Vector operations
│   └── dependency_resolver.rs # Asset dependency graphs
├── tests/
│   └── integration.rs         # Rust integration tests
├── Cargo.toml
└── README.md
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Add WASM function | `src/lib.rs` | Mark with `#[wasm_bindgen]` |
| Add TS fallback | `../src/wasm/index.ts` | Required for every WASM fn |
| Build WASM | `npm run build:wasm` | Output to `src/wasm/pkg/` |
| Run Rust tests | `cargo test` | In wasm/ directory |

## CONVENTIONS
- **Safety**: No `unsafe` blocks unless required for FFI
- **Fallbacks**: Every WASM function MUST have matching TS implementation
- **Types**: Use `wasm-bindgen` for type generation
- **f32 Precision**: Use `f32` for floating-point (performance)
- **Batching**: Batch data to minimize TS ↔ Rust FFI calls

## ANTI-PATTERNS
- **Heavy FFI**: Avoid frequent small calls; batch data instead
- **Large Binaries**: Keep `.wasm` under 500KB (minimize deps)
- **Missing Fallback**: Never add WASM fn without TS equivalent

## BUILD
```bash
cargo install wasm-pack   # Once per machine
npm run build:wasm        # Build + copy to src/wasm/pkg/
```

## PERFORMANCE
| Operation | WASM | TypeScript | Speedup |
|-----------|------|------------|---------|
| Parse 1000 props | 20-40ms | 150-300ms | 5-8x |
| Vector ops (1000) | 1-2ms | 5-10ms | 5x |
| Transform compose | 0.5ms | 5ms | 10x |
