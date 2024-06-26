#type compute
#version 450 core

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (rgba32f, binding = 0) uniform image2D heightMap;
layout (rgba32f, binding = 1) uniform image2D normalMap;

void main()
{
  vec2 uv01 = vec2(gl_GlobalInvocationID.xy) / gl_NumWorkGroups.xy;
  vec2 uv = 2 * uv01 - vec2(1.0);
  float dist = length(uv);
  float color = fract(dist);

  imageStore(heightMap, ivec2(gl_GlobalInvocationID.xy), vec4(color, color, color, 1.0));
}