//! Asset dependency resolution module
//!
//! This module provides efficient traversal and analysis of
//! asset dependency graphs.

use serde::{Deserialize, Serialize};
use std::collections::{HashSet, HashMap, VecDeque};
use wasm_bindgen::prelude::*;

/// Asset dependency information
#[derive(Serialize, Deserialize, Clone)]
#[serde(rename_all = "camelCase")]
pub struct AssetDependency {
    pub path: String,
    pub dependencies: Vec<String>,
    pub dependents: Vec<String>,
    pub depth: u32,
}

/// Dependency graph analysis result
#[derive(Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct DependencyAnalysis {
    pub asset: String,
    pub dependencies: Vec<AssetDependency>,
    pub total_dependency_count: usize,
    pub max_depth: u32,
    pub circular_dependencies: Vec<Vec<String>>,
    pub analysis_time_ms: f64,
}

#[wasm_bindgen]
pub struct DependencyResolver {
    /// Maximum depth to prevent infinite recursion
    max_depth: usize,
}

#[wasm_bindgen]
impl DependencyResolver {
    #[wasm_bindgen(constructor)]
    pub fn new(max_depth: usize) -> DependencyResolver {
        console_error_panic_hook::set_once();
        DependencyResolver { max_depth }
    }

    /// Create a DependencyResolver with default max depth (100)
    #[wasm_bindgen(constructor)]
    pub fn new() -> DependencyResolver {
        DependencyResolver::new(100)
    }

    /// Analyze dependencies for an asset
    #[wasm_bindgen(js_name = analyzeDependencies)]
    pub fn analyze_dependencies(
        &self,
        asset_path: &str,
        dependencies_json: &str,
        max_depth: Option<usize>,
    ) -> Result<JsValue, JsValue> {
        let start = js_sys::Date::now();

        let dependencies_map: HashMap<String, Vec<String>> = serde_json::from_str(dependencies_json)
            .map_err(|e| JsValue::from_str(&format!("JSON parse error: {}", e)))?;

        let depth_limit = max_depth.unwrap_or(self.max_depth);

        if !dependencies_map.contains_key(asset_path) {
            return Err(JsValue::from_str(&format!("Asset not found: {}", asset_path)));
        }

        let mut visited = HashSet::new();
        let mut result = Vec::new();
        let mut stack = VecDeque::new();
        let mut circular_deps = Vec::new();

        stack.push_back((asset_path.to_string(), 0));

        while let Some((current_path, current_depth)) = stack.pop_front() {
            if current_depth > depth_limit {
                continue;
            }

            if visited.contains(&current_path) {
                // Found circular dependency
                circular_deps.push(vec![current_path.clone()]);
                continue;
            }

            visited.insert(current_path.clone());

            if let Some(deps) = dependencies_map.get(&current_path) {
                result.push(AssetDependency {
                    path: current_path.clone(),
                    dependencies: deps.clone(),
                    dependents: self.find_dependents(&current_path, &dependencies_map),
                    depth: current_depth,
                });

                for dep in deps {
                    if !visited.contains(dep) {
                        stack.push_back((dep.clone(), current_depth + 1));
                    }
                }
            }
        }

        let analysis_time = js_sys::Date::now() - start;

        let analysis = DependencyAnalysis {
            asset: asset_path.to_string(),
            dependencies: result,
            total_dependency_count: result.len(),
            max_depth: depth_limit as u32,
            circular_dependencies: circular_deps,
            analysis_time_ms: analysis_time,
        };

        Ok(JsValue::from_serde(&analysis).map_err(|e| JsValue::from_str(&format!("Serialize error: {}", e)))?)
    }

    /// Find all dependents of an asset
    #[wasm_bindgen(js_name = findDependents)]
    pub fn find_dependents(
        &self,
        asset_path: &str,
        dependencies_json: &str,
    ) -> Result<JsValue, JsValue> {
        let dependencies_map: HashMap<String, Vec<String>> = serde_json::from_str(dependencies_json)
            .map_err(|e| JsValue::from_str(&format!("JSON parse error: {}", e)))?;

        let dependents = self.find_dependents_internal(asset_path, &dependencies_map);

        Ok(JsValue::from_serde(&dependents).map_err(|e| JsValue::from_str(&format!("Serialize error: {}", e)))?)
    }

    /// Calculate dependency depth
    #[wasm_bindgen(js_name = calculateDepth)]
    pub fn calculate_depth(
        &self,
        asset_path: &str,
        dependencies_json: &str,
        max_depth: Option<usize>,
    ) -> Result<u32, JsValue> {
        let dependencies_map: HashMap<String, Vec<String>> = serde_json::from_str(dependencies_json)
            .map_err(|e| JsValue::from_str(&format!("JSON parse error: {}", e)))?;

        let depth_limit = max_depth.unwrap_or(self.max_depth);

        let depth = self.calculate_depth_internal(asset_path, &dependencies_map, depth_limit, &mut HashSet::new());

        Ok(depth)
    }

    /// Find circular dependencies in the graph
    #[wasm_bindgen(js_name = findCircularDependencies)]
    pub fn find_circular_dependencies(
        &self,
        dependencies_json: &str,
        max_depth: Option<usize>,
    ) -> Result<JsValue, JsValue> {
        let dependencies_map: HashMap<String, Vec<String>> = serde_json::from_str(dependencies_json)
            .map_err(|e| JsValue::from_str(&format!("JSON parse error: {}", e)))?;

        let depth_limit = max_depth.unwrap_or(self.max_depth);

        let circular_deps = self.find_circular_dependencies_internal(&dependencies_map, depth_limit);

        Ok(JsValue::from_serde(&circular_deps).map_err(|e| JsValue::from_str(&format!("Serialize error: {}", e)))?)
    }

    /// Topological sort of dependencies
    #[wasm_bindgen(js_name = topologicalSort)]
    pub fn topological_sort(
        &self,
        dependencies_json: &str,
    ) -> Result<JsValue, JsValue> {
        let dependencies_map: HashMap<String, Vec<String>> = serde_json::from_str(dependencies_json)
            .map_err(|e| JsValue::from_str(&format!("JSON parse error: {}", e)))?;

        let sorted = self.topological_sort_internal(&dependencies_map)?;

        Ok(JsValue::from_serde(&sorted).map_err(|e| JsValue::from_str(&format!("Serialize error: {}", e)))?)
    }
}

impl DependencyResolver {
    fn find_dependents_internal(
        &self,
        asset_path: &str,
        dependencies_map: &HashMap<String, Vec<String>>,
    ) -> Vec<String> {
        let mut dependents = Vec::new();

        for (path, deps) in dependencies_map {
            if deps.contains(asset_path) {
                dependents.push(path.clone());
            }
        }

        dependents
    }

    fn calculate_depth_internal(
        &self,
        asset_path: &str,
        dependencies_map: &HashMap<String, Vec<String>>,
        max_depth: usize,
        visited: &mut HashSet<String>,
    ) -> u32 {
        if visited.contains(asset_path) {
            return 0;
        }

        if max_depth == 0 {
            return 0;
        }

        visited.insert(asset_path.to_string());

        if let Some(deps) = dependencies_map.get(asset_path) {
            if deps.is_empty() {
                visited.remove(asset_path);
                return 0;
            }

            let mut max_child_depth = 0;
            for dep in deps {
                let child_depth = self.calculate_depth_internal(
                    dep,
                    dependencies_map,
                    max_depth - 1,
                    visited,
                );
                max_child_depth = max_child_depth.max(child_depth);
            }

            visited.remove(asset_path);
            max_child_depth + 1
        } else {
            visited.remove(asset_path);
            0
        }
    }

    fn find_circular_dependencies_internal(
        &self,
        dependencies_map: &HashMap<String, Vec<String>>,
        max_depth: usize,
    ) -> Vec<Vec<String>> {
        let mut cycles = Vec::new();
        let mut visited = HashSet::new();
        let mut stack = HashSet::new();

        fn dfs(
            current: &str,
            dependencies_map: &HashMap<String, Vec<String>>,
            visited: &mut HashSet<String>,
            stack: &mut HashSet<String>,
            cycles: &mut Vec<Vec<String>>,
            path: &mut Vec<String>,
            depth: usize,
            max_depth: usize,
        ) {
            if depth > max_depth {
                return;
            }

            visited.insert(current.to_string());
            stack.insert(current.to_string());
            path.push(current.to_string());

            if let Some(deps) = dependencies_map.get(current) {
                for dep in deps {
                    if !visited.contains(dep) {
                        dfs(
                            dep,
                            dependencies_map,
                            visited,
                            stack,
                            cycles,
                            path,
                            depth + 1,
                            max_depth,
                        );
                    } else if stack.contains(dep) {
                        // Found a cycle
                        let cycle_start = path.iter().position(|p| p == dep);
                        if let Some(start) = cycle_start {
                            cycles.push(path[start..].to_vec());
                        }
                    }
                }
            }

            stack.remove(current);
            path.pop();
        }

        for asset in dependencies_map.keys() {
            if !visited.contains(asset) {
                let mut path = Vec::new();
                dfs(
                    asset,
                    dependencies_map,
                    &mut visited,
                    &mut stack,
                    &mut cycles,
                    &mut path,
                    0,
                    max_depth,
                );
            }
        }

        cycles
    }

    fn topological_sort_internal(
        &self,
        dependencies_map: &HashMap<String, Vec<String>>,
    ) -> Result<Vec<String>, JsValue> {
        let mut in_degree: HashMap<String, usize> = HashMap::new();
        let mut graph: HashMap<String, Vec<String>> = HashMap::new();

        // Initialize
        for (asset, deps) in dependencies_map {
            in_degree.entry(asset.clone()).or_insert(0);
            for dep in deps {
                in_degree.entry(dep.clone()).or_insert(0);
                graph.entry(dep.clone()).or_insert_with(Vec::new).push(asset.clone());
            }
        }

        // Kahn's algorithm
        let mut queue: VecDeque<String> = in_degree
            .iter()
            .filter(|(_, &degree)| degree == 0)
            .map(|(&asset, _)| asset)
            .collect();

        let mut sorted = Vec::new();

        while let Some(asset) = queue.pop_front() {
            sorted.push(asset.clone());

            if let Some(dependents) = graph.get(&asset) {
                for dependent in dependents {
                    if let Some(&mut ref mut degree) = in_degree.get_mut(dependent) {
                        *degree -= 1;
                        if *degree == 0 {
                            queue.push_back(dependent.clone());
                        }
                    }
                }
            }
        }

        if sorted.len() != in_degree.len() {
            return Err(JsValue::from_str("Graph contains cycles - topological sort not possible"));
        }

        Ok(sorted)
    }
}
