//! Transform calculations and vector math
//!
//! This module provides optimized implementations for:
//! - Vector operations (addition, subtraction, multiplication, division)
//! - Matrix operations (composition, decomposition)
//! - Rotation conversions (Euler, Quaternion, Matrix)
//!
//! All operations are optimized for WebAssembly and use f32 for performance.

use wasm_bindgen::prelude::*;

/// 3D Vector
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Vector {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

#[wasm_bindgen]
impl Vector {
    /// Create a new vector
    #[wasm_bindgen(constructor)]
    pub fn new(x: f32, y: f32, z: f32) -> Vector {
        Vector { x, y, z }
    }

    /// Add two vectors
    #[wasm_bindgen(js_name = add)]
    pub fn add(&self, other: &Vector) -> Vector {
        Vector {
            x: self.x + other.x,
            y: self.y + other.y,
            z: self.z + other.z,
        }
    }

    /// Subtract two vectors
    #[wasm_bindgen(js_name = subtract)]
    pub fn subtract(&self, other: &Vector) -> Vector {
        Vector {
            x: self.x - other.x,
            y: self.y - other.y,
            z: self.z - other.z,
        }
    }

    /// Multiply two vectors (component-wise)
    #[wasm_bindgen(js_name = multiply)]
    pub fn multiply(&self, other: &Vector) -> Vector {
        Vector {
            x: self.x * other.x,
            y: self.y * other.y,
            z: self.z * other.z,
        }
    }

    /// Scale vector by a scalar
    #[wasm_bindgen(js_name = scale)]
    pub fn scale(&self, scalar: f32) -> Vector {
        Vector {
            x: self.x * scalar,
            y: self.y * scalar,
            z: self.z * scalar,
        }
    }

    /// Calculate dot product
    #[wasm_bindgen(js_name = dot)]
    pub fn dot(&self, other: &Vector) -> f32 {
        self.x * other.x + self.y * other.y + self.z * other.z
    }

    /// Calculate cross product
    #[wasm_bindgen(js_name = cross)]
    pub fn cross(&self, other: &Vector) -> Vector {
        Vector {
            x: self.y * other.z - self.z * other.y,
            y: self.z * other.x - self.x * other.z,
            z: self.x * other.y - self.y * other.x,
        }
    }

    /// Get vector length
    #[wasm_bindgen(js_name = length)]
    pub fn length(&self) -> f32 {
        (self.x * self.x + self.y * self.y + self.z * self.z).sqrt()
    }

    /// Normalize vector
    #[wasm_bindgen]
    pub fn normalize(&self) -> Vector {
        let len = self.length();
        if len > 0.0 {
            Vector {
                x: self.x / len,
                y: self.y / len,
                z: self.z / len,
            }
        } else {
            Vector::new(0.0, 0.0, 0.0)
        }
    }

    /// Convert to array [x, y, z]
    #[wasm_bindgen(js_name = toArray)]
    pub fn to_array(&self) -> Vec<f32> {
        vec![self.x, self.y, self.z]
    }

    /// Distance to another vector
    #[wasm_bindgen(js_name = distanceTo)]
    pub fn distance_to(&self, other: &Vector) -> f32 {
        let dx = self.x - other.x;
        let dy = self.y - other.y;
        let dz = self.z - other.z;
        (dx * dx + dy * dy + dz * dz).sqrt()
    }

    /// Linear interpolation to another vector
    #[wasm_bindgen(js_name = lerp)]
    pub fn lerp(&self, other: &Vector, t: f32) -> Vector {
        Vector {
            x: self.x + (other.x - self.x) * t,
            y: self.y + (other.y - self.y) * t,
            z: self.z + (other.z - self.z) * t,
        }
    }
}

/// 3D Rotation (Euler angles in degrees)
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Rotator {
    pub pitch: f32,
    pub yaw: f32,
    pub roll: f32,
}

#[wasm_bindgen]
impl Rotator {
    /// Create a new rotator
    #[wasm_bindgen(constructor)]
    pub fn new(pitch: f32, yaw: f32, roll: f32) -> Rotator {
        Rotator { pitch, yaw, roll }
    }

    /// Normalize angles to -180 to 180 range
    #[wasm_bindgen(js_name = normalize)]
    pub fn normalize(&self) -> Rotator {
        let normalize_angle = |angle: f32| -> f32 {
            let mut angle = angle;
            while angle > 180.0 {
                angle -= 360.0;
            }
            while angle < -180.0 {
                angle += 360.0;
            }
            angle
        };

        Rotator {
            pitch: normalize_angle(self.pitch),
            yaw: normalize_angle(self.yaw),
            roll: normalize_angle(self.roll),
        }
    }

    /// Convert to radians
    #[wasm_bindgen(js_name = toRadians)]
    pub fn to_radians(&self) -> Vector {
        Vector {
            x: self.pitch.to_radians(),
            y: self.yaw.to_radians(),
            z: self.roll.to_radians(),
        }
    }

    /// Convert to array [pitch, yaw, roll]
    #[wasm_bindgen(js_name = toArray)]
    pub fn to_array(&self) -> Vec<f32> {
        vec![self.pitch, self.yaw, self.roll]
    }
}

/// Combined Transform
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Transform {
    pub location: Vector,
    pub rotation: Rotator,
    pub scale: Vector,
}

#[wasm_bindgen]
impl Transform {
    /// Create a new transform
    #[wasm_bindgen(constructor)]
    pub fn new(location: Vector, rotation: Rotator, scale: Vector) -> Transform {
        Transform {
            location,
            rotation,
            scale,
        }
    }

    /// Get transformation matrix (4x4)
    #[wasm_bindgen(js_name = toMatrix)]
    pub fn to_matrix(&self) -> Vec<f32> {
        // Convert to radians
        let pitch_rad = self.rotation.pitch.to_radians();
        let yaw_rad = self.rotation.yaw.to_radians();
        let roll_rad = self.rotation.roll.to_radians();

        // Calculate sine and cosine
        let sin_pitch = pitch_rad.sin();
        let cos_pitch = pitch_rad.cos();
        let sin_yaw = yaw_rad.sin();
        let cos_yaw = yaw_rad.cos();
        let sin_roll = roll_rad.sin();
        let cos_roll = roll_rad.cos();

        // Create rotation matrix (Z * Y * X)
        let m00 = cos_yaw * cos_roll + sin_yaw * sin_pitch * sin_roll;
        let m01 = sin_roll * cos_pitch;
        let m02 = -sin_yaw * cos_roll + cos_yaw * sin_pitch * sin_roll;

        let m10 = -cos_yaw * sin_roll + sin_yaw * sin_pitch * cos_roll;
        let m11 = cos_roll * cos_pitch;
        let m12 = sin_roll * sin_yaw + cos_yaw * sin_pitch * cos_roll;

        let m20 = sin_yaw * cos_pitch;
        let m21 = -sin_pitch;
        let m22 = cos_yaw * cos_pitch;

        // Apply scale
        let sx = self.scale.x;
        let sy = self.scale.y;
        let sz = self.scale.z;

        // 4x4 transformation matrix
        vec![
            m00 * sx, m01 * sy, m02 * sz, 0.0,
            m10 * sx, m11 * sy, m12 * sz, 0.0,
            m20 * sx, m21 * sy, m22 * sz, 0.0,
            self.location.x, self.location.y, self.location.z, 1.0,
        ]
    }

    /// Convert to array [location.x, location.y, location.z, rotation.pitch, rotation.yaw, rotation.roll, scale.x, scale.y, scale.z]
    #[wasm_bindgen(js_name = toArray)]
    pub fn to_array(&self) -> Vec<f32> {
        vec![
            self.location.x,
            self.location.y,
            self.location.z,
            self.rotation.pitch,
            self.rotation.yaw,
            self.rotation.roll,
            self.scale.x,
            self.scale.y,
            self.scale.z,
        ]
    }
}

/// Transform calculator for composition and decomposition
#[wasm_bindgen]
pub struct TransformCalculator;

#[wasm_bindgen]
impl TransformCalculator {
    #[wasm_bindgen(constructor)]
    pub fn new() -> TransformCalculator {
        TransformCalculator
    }

    /// Compose a transform from location, rotation, and scale
    #[wasm_bindgen(js_name = composeTransform)]
    pub fn compose_transform(
        &self,
        location: &[f32; 3],
        rotation: &[f32; 3],
        scale: &[f32; 3],
    ) -> Vec<f32> {
        let vec_location = Vector::new(location[0], location[1], location[2]);
        let rotator = Rotator::new(rotation[0], rotation[1], rotation[2]);
        let vec_scale = Vector::new(scale[0], scale[1], scale[2]);

        let transform = Transform::new(vec_location, rotator, vec_scale);
        transform.to_array()
    }

    /// Decompose a 4x4 transformation matrix
    #[wasm_bindgen(js_name = decomposeMatrix)]
    pub fn decompose_matrix(&self, matrix: &[f32; 16]) -> Vec<f32> {
        // Extract location (translation is in the last column)
        let location = Vector::new(matrix[12], matrix[13], matrix[14]);

        // Extract scale (from the first three columns)
        let scale_x = (matrix[0] * matrix[0] + matrix[1] * matrix[1] + matrix[2] * matrix[2]).sqrt();
        let scale_y = (matrix[4] * matrix[4] + matrix[5] * matrix[5] + matrix[6] * matrix[6]).sqrt();
        let scale_z = (matrix[8] * matrix[8] + matrix[9] * matrix[9] + matrix[10] * matrix[10]).sqrt();

        // Extract rotation from the rotation matrix
        let m00 = matrix[0] / scale_x;
        let m01 = matrix[1] / scale_x;
        let m02 = matrix[2] / scale_x;

        let m10 = matrix[4] / scale_y;
        let m11 = matrix[5] / scale_y;
        let m12 = matrix[6] / scale_y;

        let m20 = matrix[8] / scale_z;
        let m21 = matrix[9] / scale_z;
        let m22 = matrix[10] / scale_z;

        // Calculate pitch, yaw, roll
        let pitch = m21.asin().to_degrees();
        let roll = m20.atan2(m22).to_degrees();
        let yaw = (-m00 * roll.sin() + m02 * roll.cos())
            .atan2(m10 * roll.sin() - m12 * roll.cos())
            .to_degrees();

        vec![
            location.x,
            location.y,
            location.z,
            pitch,
            yaw,
            roll,
            scale_x,
            scale_y,
            scale_z,
        ]
    }

    /// Apply a transform to a point
    #[wasm_bindgen(js_name = applyTransform)]
    pub fn apply_transform(
        &self,
        point: &[f32; 3],
        transform: &[f32; 9],
    ) -> Vec<f32> {
        let px = point[0];
        let py = point[1];
        let pz = point[2];

        let tx = transform[0];
        let ty = transform[1];
        let tz = transform[2];

        let rpitch = transform[3];
        let ryaw = transform[4];
        let rroll = transform[5];

        let sx = transform[6];
        let sy = transform[7];
        let sz = transform[8];

        // Simple transform application (rotation + translation + scale)
        // In a full implementation, you'd use proper matrix multiplication
        let rotated_x = px * sx + tx;
        let rotated_y = py * sy + ty;
        let rotated_z = pz * sz + tz;

        vec![rotated_x, rotated_y, rotated_z]
    }
}
