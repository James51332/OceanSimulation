#section type(vertex)
#version 450 core

layout(location = 0) in vec3 a_Pos;

out vec3 texCoord;

layout(binding = 0) uniform pushConstants
{
  mat4 u_View;
  mat4 u_Projection;
  mat4 u_ViewProjection;
  vec2 u_ViewportSize;
  float u_Time;
  float dummy; // 16 byte alignment
};

void main()
{
  texCoord = a_Pos;

  mat4 noTranslateView = mat4(mat3(u_View));
  vec4 pos = u_Projection * noTranslateView * vec4(a_Pos, 1.0);
  gl_Position = pos.xyww; // place the depth in normalized coords as far as possible.
}

#section type(fragment)
#version 450 core

in vec3 texCoord;

out vec4 FragColor;

layout(binding = 0) uniform samplerCube skybox;

void main()
{
  FragColor = texture(skybox, texCoord);
}