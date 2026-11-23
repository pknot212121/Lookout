struct GlobalUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
};

struct TileData {
    z: u32,
    x: u32,
    y: u32,
    layer: u32,
};

@group(0) @binding(0) var<uniform> globalUniforms: GlobalUniforms;
@group(0) @binding(1) var<storage, read> tiles: array<TileData>;
@group(0) @binding(2) var myTexture: texture_2d_array<f32>;
@group(0) @binding(3) var mySampler: sampler;

const PI: f32 = 3.14159265359;
const RADIUS: f32 = 100.0;

struct VertexInput {
    @builtin(instance_index) instanceIdx: u32,
    @location(0) position: vec2f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) @interpolate(flat) layer: u32,
};

@vertex
fn vs(in: VertexInput) -> VertexOutput {
    let tile = tiles[in.instanceIdx];

    let n = f32(1u << tile.z);
    let u = (f32(tile.x) + in.position.x) / n;
    let v = (f32(tile.y) + in.position.y) / n;

    let lon = (u - 0.5) * 2.0 * PI;
    let lat = atan(sinh(PI * (1.0 - 2.0 * v)));

    let p = vec3f(
        cos(lat) * cos(lon),
        sin(lat),
        cos(lat) * sin(lon)
    ) * RADIUS;

    var out: VertexOutput;
    out.position = globalUniforms.projectionMatrix * globalUniforms.viewMatrix * vec4f(p, 1.0);
    out.uv = in.position;
    out.layer = tile.layer;
    return out;
}

@fragment
fn fs(in: VertexOutput) -> @location(0) vec4f {
    return textureSample(myTexture, mySampler, in.uv, in.layer);
}