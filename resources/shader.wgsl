struct GlobalUniforms 
{
	projectionMatrix: mat4x4f,
	viewMatrix: mat4x4f,
};

@group(0) @binding(0) var<uniform> globalUniforms: GlobalUniforms;

@group(0) @binding(1) var<storage, read> instanceModels: array<mat4x4f>;

@group(0) @binding(2) var myTexture: texture_2d<f32>;
@group(0) @binding(3) var mySampler: sampler;

struct VertexInput
{
	@builtin(instance_index) instanceIdx: u32,
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
};

struct VertexOutput
{
	@builtin(position) position: vec4f,
	@location(0) uv: vec2f,
};

@vertex
fn vs(in: VertexInput) -> VertexOutput
{
	var out: VertexOutput;
	let modelMatrix = instanceModels[in.instanceIdx];
	let worldPos = modelMatrix * vec4f(in.position, 1.0);
	out.position = globalUniforms.projectionMatrix * globalUniforms.viewMatrix * worldPos;
	out.uv = in.uv;
	return out;
}

@fragment
fn fs(in: VertexOutput) -> @location(0) vec4f
{
	return textureSample(myTexture, mySampler, in.uv);
}