use wasm_bindgen::prelude::*;

#[wasm_bindgen]
pub struct MeshAnalyzer;

#[wasm_bindgen]
impl MeshAnalyzer {
    #[wasm_bindgen(constructor)]
    pub fn new() -> MeshAnalyzer {
        MeshAnalyzer
    }

    /// Calculate axis-aligned bounding box from vertex array [x, y, z, x, y, z, ...]
    #[wasm_bindgen(js_name = calculateBounds)]
    pub fn calculate_bounds(&self, vertices: &[f32]) -> Vec<f32> {
        if vertices.is_empty() {
            return vec![0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
        }

        let mut min_x = f32::INFINITY;
        let mut min_y = f32::INFINITY;
        let mut min_z = f32::INFINITY;
        let mut max_x = f32::NEG_INFINITY;
        let mut max_y = f32::NEG_INFINITY;
        let mut max_z = f32::NEG_INFINITY;

        for chunk in vertices.chunks(3) {
            if chunk.len() == 3 {
                let x = chunk[0];
                let y = chunk[1];
                let z = chunk[2];

                if x < min_x { min_x = x; }
                if y < min_y { min_y = y; }
                if z < min_z { min_z = z; }
                if x > max_x { max_x = x; }
                if y > max_y { max_y = y; }
                if z > max_z { max_z = z; }
            }
        }

        vec![min_x, min_y, min_z, max_x, max_y, max_z]
    }

    /// Calculate surface area of a mesh
    #[wasm_bindgen(js_name = calculateSurfaceArea)]
    pub fn calculate_surface_area(&self, vertices: &[f32], indices: &[u32]) -> f32 {
        let mut total_area = 0.0;

        for chunk in indices.chunks(3) {
            if chunk.len() == 3 {
                let idx0 = chunk[0] as usize * 3;
                let idx1 = chunk[1] as usize * 3;
                let idx2 = chunk[2] as usize * 3;

                if idx0 + 2 < vertices.len() && idx1 + 2 < vertices.len() && idx2 + 2 < vertices.len() {
                    let v0_x = vertices[idx0];
                    let v0_y = vertices[idx0 + 1];
                    let v0_z = vertices[idx0 + 2];

                    let v1_x = vertices[idx1];
                    let v1_y = vertices[idx1 + 1];
                    let v1_z = vertices[idx1 + 2];

                    let v2_x = vertices[idx2];
                    let v2_y = vertices[idx2 + 1];
                    let v2_z = vertices[idx2 + 2];

                    // Cross product of edges
                    let e1_x = v1_x - v0_x;
                    let e1_y = v1_y - v0_y;
                    let e1_z = v1_z - v0_z;

                    let e2_x = v2_x - v0_x;
                    let e2_y = v2_y - v0_y;
                    let e2_z = v2_z - v0_z;

                    let cross_x = e1_y * e2_z - e1_z * e2_y;
                    let cross_y = e1_z * e2_x - e1_x * e2_z;
                    let cross_z = e1_x * e2_y - e1_y * e2_x;

                    let area = 0.5 * (cross_x * cross_x + cross_y * cross_y + cross_z * cross_z).sqrt();
                    total_area += area;
                }
            }
        }

        total_area
    }
}
