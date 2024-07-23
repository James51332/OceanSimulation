#section type(vertex)
#version 450 core

layout (location = 0) in vec3 a_Pos;
layout (location = 3) in vec2 a_UV;

layout (binding = 0) uniform pushConstants
{
  mat4 u_View;
  mat4 u_Projection;
  mat4 u_ViewProjection;
  vec2 u_ViewportSize;
  float u_Time;
  float dummy; // 16 byte alignment
};

out vec2 v_UV;


void main()
{ 
  gl_Position = u_ViewProjection * vec4(a_Pos, 1.0);
  v_UV = a_UV;
}

#section type(fragment)
#version 450 core

in vec2 v_UV;

layout (binding = 0) uniform sampler2D heightMap;

out vec4 fragColor;

void main()
{
  float height = texture(heightMap, v_UV).r / 4.0 + 0.5;
  fragColor = vec4(0.0, 0.0, height, 1.0);
}