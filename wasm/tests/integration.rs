//! Integration tests for WASM module

use wasm_bindgen_test::*;
use unreal_mcp_wasm::*;

wasm_bindgen_test_configure!(run_in_browser);

#[wasm_bindgen_test]
fn test_property_parser() {
    let parser = PropertyParser::new();

    let json = r#"{
        "name": "TestAsset",
        "value": 42,
        "location": {
            "x": 100,
            "y": 200,
            "z": 300
        },
        "tags": ["tag1", "tag2"]
    }"#;

    let result = parser.parse_properties(json, None);
    assert!(result.is_ok());
}

#[wasm_bindgen_test]
fn test_vector_operations() {
    let v1 = Vector::new(1.0, 2.0, 3.0);
    let v2 = Vector::new(4.0, 5.0, 6.0);

    let sum = v1.add(&v2);
    assert_eq!(sum.x, 5.0);
    assert_eq!(sum.y, 7.0);
    assert_eq!(sum.z, 9.0);

    let dot = v1.dot(&v2);
    assert_eq!(dot, 32.0);

    let length = v1.length();
    assert!(length > 0.0);
}

#[wasm_bindgen_test]
fn test_transform_calculator() {
    let calculator = TransformCalculator::new();

    let location = [100.0, 200.0, 300.0];
    let rotation = [0.0, 90.0, 0.0];
    let scale = [1.0, 1.0, 1.0];

    let matrix = calculator.compose_transform(&location, &rotation, &scale);
    assert_eq!(matrix.len(), 9);

    // Verify location is preserved
    assert_eq!(matrix[0], location[0]);
    assert_eq!(matrix[1], location[1]);
    assert_eq!(matrix[2], location[2]);
}

#[wasm_bindgen_test]
fn test_dependency_resolver() {
    let resolver = DependencyResolver::new();

    let json = r#"{
        "AssetA": ["AssetB", "AssetC"],
        "AssetB": ["AssetC"],
        "AssetC": []
    }"#;

    let result = resolver.analyze_dependencies("AssetA", json, None);
    assert!(result.is_ok());
}

#[wasm_bindgen_test]
fn test_utils() {
    assert_eq!(Utils::clamp(5.0, 0.0, 10.0), 5.0);
    assert_eq!(Utils::clamp(-5.0, 0.0, 10.0), 0.0);
    assert_eq!(Utils::clamp(15.0, 0.0, 10.0), 10.0);

    assert!((Utils::lerp(0.0, 10.0, 0.5) - 5.0).abs() < 0.001);

    let rad = Utils::deg_to_rad(180.0);
    assert!((rad - std::f32::consts::PI).abs() < 0.001);

    let deg = Utils::rad_to_deg(std::f32::consts::PI);
    assert!((deg - 180.0).abs() < 0.001);
}

#[wasm_bindgen_test]
fn test_metrics() {
    let mut metrics = Metrics::new();

    metrics.record("test_op".to_string(), 10.0);
    metrics.record("test_op".to_string(), 20.0);
    metrics.record("other_op".to_string(), 15.0);

    let avg_test = metrics.average_for("test_op".to_string());
    assert_eq!(avg_test, 15.0);

    let avg_other = metrics.average_for("other_op".to_string());
    assert_eq!(avg_other, 15.0);

    let count = metrics.count();
    assert_eq!(count, 3);

    metrics.clear();
    assert_eq!(metrics.count(), 0);
}
