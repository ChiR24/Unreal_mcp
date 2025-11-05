//! WebAssembly module for performance-critical operations
//! in the Unreal Engine MCP Server.
//!
//! This module provides optimized implementations for:
//! - Property parsing and serialization
//! - Transform calculations
//! - Vector math
//! - Asset dependency resolution

mod property_parser;
mod transform_math;
mod dependency_resolver;

pub use property_parser::PropertyParser;
pub use transform_math::{TransformCalculator, Vector, Rotator, Transform};
pub use dependency_resolver::DependencyResolver;

// Re-export wasm-bindgen for convenience
pub use wasm_bindgen::prelude::*;

/// Initialize the WASM module
#[wasm_bindgen(start)]
pub fn main() {
    // This function is called when the module is loaded
    console_error_panic_hook::set_once();
    web_sys::console::log_1(&"WASM module initialized".into());
}

// Provide console logging in WASM
#[wasm_bindgen]
extern "C" {
    #[wasm_bindgen(js_namespace = console)]
    fn log(s: &str);

    #[wasm_bindgen(js_namespace = console)]
    fn error(s: &str);

    #[wasm_bindgen(js_namespace = console)]
    fn warn(s: &str);
}

/// Utility functions
#[wasm_bindgen]
pub struct Utils;

#[wasm_bindgen]
impl Utils {
    /// Get the current high-resolution time in milliseconds
    #[wasm_bindgen(js_name = now)]
    pub fn now() -> f64 {
        web_sys::window()
            .and_then(|w| w.performance())
            .map(|p| p.now())
            .unwrap_or(0.0)
    }

    /// Generate a random floating-point number between 0 and 1
    #[wasm_bindgen(js_name = random)]
    pub fn random() -> f64 {
        js_sys::Math::random()
    }

    /// Clamp a number between min and max
    #[wasm_bindgen(js_name = clamp)]
    pub fn clamp(value: f32, min: f32, max: f32) -> f32 {
        value.max(min).min(max)
    }

    /// Linear interpolation between two values
    #[wasm_bindgen(js_name = lerp)]
    pub fn lerp(a: f32, b: f32, t: f32) -> f32 {
        a + (b - a) * t
    }

    /// Convert degrees to radians
    #[wasm_bindgen(js_name = degToRad)]
    pub fn deg_to_rad(degrees: f32) -> f32 {
        degrees * std::f32::consts::PI / 180.0
    }

    /// Convert radians to degrees
    #[wasm_bindgen(js_name = radToDeg)]
    pub fn rad_to_deg(radians: f32) -> f32 {
        radians * 180.0 / std::f32::consts::PI
    }
}

/// Performance metrics for WASM operations
#[wasm_bindgen]
pub struct Metrics {
    operations: Vec<OperationMetric>,
}

#[derive(Clone)]
struct OperationMetric {
    name: String,
    duration_ms: f64,
    timestamp: f64,
}

#[wasm_bindgen]
impl Metrics {
    #[wasm_bindgen(constructor)]
    pub fn new() -> Metrics {
        Metrics {
            operations: Vec::new(),
        }
    }

    /// Record an operation
    #[wasm_bindgen(js_name = record)]
    pub fn record(&mut self, name: String, duration_ms: f64) {
        let timestamp = js_sys::Date::now();
        self.operations.push(OperationMetric {
            name,
            duration_ms,
            timestamp,
        });
    }

    /// Get average duration for an operation
    #[wasm_bindgen(js_name = averageFor)]
    pub fn average_for(&self, name: String) -> f64 {
        let mut total = 0.0;
        let mut count = 0.0;

        for op in &self.operations {
            if op.name == name {
                total += op.duration_ms;
                count += 1.0;
            }
        }

        if count > 0.0 {
            total / count
        } else {
            0.0
        }
    }

    /// Get total number of operations recorded
    #[wasm_bindgen]
    pub fn count(&self) -> u32 {
        self.operations.len() as u32
    }

    /// Clear all metrics
    #[wasm_bindgen]
    pub fn clear(&mut self) {
        self.operations.clear();
    }
}
