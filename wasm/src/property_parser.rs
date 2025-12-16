//! Property parsing and serialization module
//!
//! This module provides efficient parsing and serialization of
//! Unreal Engine property data structures.

use serde::{Serialize, Deserialize};
use serde_json::{Value, Map};
use wasm_bindgen::prelude::*;
use serde_wasm_bindgen as swb;

/// A parsed property with metadata
#[derive(Serialize, Deserialize, Clone)]
#[serde(rename_all = "camelCase")]
pub struct ParsedProperty {
    pub name: String,
    pub value: Value,
    pub property_type: String,
    pub is_array: bool,
    pub nested_properties: Option<Vec<ParsedProperty>>,
}

/// Result of parsing properties
#[derive(Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ParseResult {
    pub properties: Vec<ParsedProperty>,
    pub total_count: usize,
    pub parse_time_ms: f64,
}

#[wasm_bindgen]
pub struct PropertyParser {
    /// Maximum nesting depth to prevent stack overflow
    max_depth: usize,
}

#[wasm_bindgen]
impl PropertyParser {
    /// Create a new PropertyParser with default max depth (100)
    #[wasm_bindgen(constructor)]
    pub fn new() -> PropertyParser {
        console_error_panic_hook::set_once();
        PropertyParser { max_depth: 100 }
    }

    /// Create a PropertyParser with custom max depth
    #[wasm_bindgen(js_name = withMaxDepth)]
    pub fn with_max_depth(max_depth: usize) -> PropertyParser {
        console_error_panic_hook::set_once();
        PropertyParser { max_depth }
    }

    /// Parse JSON string into structured properties
    #[wasm_bindgen]
    pub fn parse_properties(&self, json_str: &str, max_depth: Option<usize>) -> Result<JsValue, JsValue> {
        let start = js_sys::Date::now();

        let value: Value = serde_json::from_str(json_str)
            .map_err(|e| JsValue::from_str(&format!("JSON parse error: {}", e)))?;

        let depth_limit = max_depth.unwrap_or(self.max_depth);
        let properties = self.parse_value(&value, "root", depth_limit)?;

        let parse_time = js_sys::Date::now() - start;

        let total_count = self.count_properties(&properties);

        let result = ParseResult {
            properties,
            total_count,
            parse_time_ms: parse_time,
        };

        let js_value = swb::to_value(&result)
            .map_err(|e| JsValue::from_str(&format!("Serialize error: {}", e)))?;
        Ok(js_value)
    }

    /// Serialize properties back to JSON
    #[wasm_bindgen]
    pub fn serialize_properties(&self, properties_js: &JsValue) -> Result<String, JsValue> {
        let properties: Vec<ParsedProperty> = swb::from_value(properties_js.clone())
            .map_err(|e| JsValue::from_str(&format!("Deserialize error: {}", e)))?;

        let value = self.properties_to_value(&properties);
        serde_json::to_string_pretty(&value)
            .map_err(|e| JsValue::from_str(&format!("Serialize error: {}", e)))
    }

    /// Extract all property names from JSON
    #[wasm_bindgen]
    pub fn extract_property_names(&self, json_str: &str) -> Result<JsValue, JsValue> {
        let value: Value = serde_json::from_str(json_str)
            .map_err(|e| JsValue::from_str(&format!("JSON parse error: {}", e)))?;

        let mut names = Vec::new();
        self.extract_names(&value, &mut names);

        let js_value = swb::to_value(&names)
            .map_err(|e| JsValue::from_str(&format!("Serialize error: {}", e)))?;
        Ok(js_value)
    }

    /// Get property type from JSON value
    #[wasm_bindgen(js_name = getPropertyType)]
    pub fn get_property_type(&self, json_str: &str) -> Result<String, JsValue> {
        let value: Value = serde_json::from_str(json_str)
            .map_err(|e| JsValue::from_str(&format!("JSON parse error: {}", e)))?;

        let prop_type = self.infer_property_type(&value);
        Ok(prop_type)
    }
}

impl PropertyParser {
    fn parse_value(&self, value: &Value, name: &str, depth: usize) -> Result<Vec<ParsedProperty>, JsValue> {
        if depth == 0 {
            return Err(JsValue::from_str("Maximum nesting depth exceeded"));
        }

        let prop_type = self.infer_property_type(value);

        let mut properties = Vec::new();

        match value {
            Value::Null => {
                properties.push(ParsedProperty {
                    name: name.to_string(),
                    value: value.clone(),
                    property_type: prop_type,
                    is_array: false,
                    nested_properties: None,
                });
            }
            Value::Bool(_) | Value::Number(_) | Value::String(_) => {
                properties.push(ParsedProperty {
                    name: name.to_string(),
                    value: value.clone(),
                    property_type: prop_type,
                    is_array: false,
                    nested_properties: None,
                });
            }
            Value::Array(arr) => {
                let mut nested = Vec::new();
                for (i, item) in arr.iter().enumerate() {
                    let item_name = format!("{}[{}]", name, i);
                    let mut item_properties = self.parse_value(item, &item_name, depth - 1)?;
                    nested.append(&mut item_properties);
                }

                properties.push(ParsedProperty {
                    name: name.to_string(),
                    value: value.clone(),
                    property_type: prop_type,
                    is_array: true,
                    nested_properties: Some(nested),
                });
            }
            Value::Object(obj) => {
                let mut nested = Vec::new();
                for (key, val) in obj {
                    let nested_properties = self.parse_value(val, key, depth - 1)?;
                    nested.extend(nested_properties);
                }

                properties.push(ParsedProperty {
                    name: name.to_string(),
                    value: value.clone(),
                    property_type: prop_type,
                    is_array: false,
                    nested_properties: Some(nested),
                });
            }
        }

        Ok(properties)
    }

    fn infer_property_type(&self, value: &Value) -> String {
        match value {
            Value::Null => "null".to_string(),
            Value::Bool(_) => "boolean".to_string(),
            Value::Number(n) => {
                if n.is_f64() {
                    "float".to_string()
                } else {
                    "integer".to_string()
                }
            }
            Value::String(_) => "string".to_string(),
            Value::Array(_) => "array".to_string(),
            Value::Object(_) => "object".to_string(),
        }
    }

    fn extract_names(&self, value: &Value, names: &mut Vec<String>) {
        match value {
            Value::Object(obj) => {
                for (key, val) in obj {
                    names.push(key.clone());
                    self.extract_names(val, names);
                }
            }
            Value::Array(arr) => {
                for item in arr {
                    self.extract_names(item, names);
                }
            }
            _ => {}
        }
    }

    fn count_properties(&self, properties: &[ParsedProperty]) -> usize {
        let mut count = properties.len();
        for prop in properties {
            if let Some(nested) = &prop.nested_properties {
                count += self.count_properties(nested);
            }
        }
        count
    }

    fn properties_to_value(&self, properties: &[ParsedProperty]) -> Value {
        // This is a simplified conversion - in practice, you'd want more sophisticated handling
        let mut map = Map::new();

        for prop in properties {
            let val = if let Some(nested) = &prop.nested_properties {
                if nested.is_empty() {
                    prop.value.clone()
                } else {
                    self.nested_to_value(nested)
                }
            } else {
                prop.value.clone()
            };
            map.insert(prop.name.clone(), val);
        }

        Value::Object(map)
    }

    fn nested_to_value(&self, properties: &[ParsedProperty]) -> Value {
        if properties.is_empty() {
            return Value::Null;
        }

        if properties.len() == 1 {
            let prop = &properties[0];
            if let Some(nested) = &prop.nested_properties {
                let mut map = Map::new();
                map.insert(prop.name.clone(), self.nested_to_value(nested));
                Value::Object(map)
            } else {
                prop.value.clone()
            }
        } else {
            let mut map = Map::new();
            for prop in properties {
                let val = if let Some(nested) = &prop.nested_properties {
                    self.nested_to_value(nested)
                } else {
                    prop.value.clone()
                };
                map.insert(prop.name.clone(), val);
            }
            Value::Object(map)
        }
    }
}
