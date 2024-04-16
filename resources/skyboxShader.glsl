#type vertex
#version 410 core

layout (location = 0) in vec3 a_Pos;

out vec3 texCoord;

uniform mat4 u_View;
uniform mat4 u_Projection;

void main()
{
  texCoord = a_Pos;
  
  mat4 noTranslateView = mat4(mat3(u_View));
  vec4 pos = u_Projection * noTranslateView * vec4(a_Pos, 1.0);
  gl_Position = pos.xyww; // place the depth in normalized coords as far as possible.
}

#type fragment
#version 410 core

in vec3 texCoord;

out vec4 FragColor;

uniform samplerCube skybox;

void main()
{
  FragColor = texture(skybox, texCoord);
}