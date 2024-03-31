#type vertex
#version 410 core

layout (location = 0) in vec3 a_Pos;

uniform mat4 u_ViewProjection;

void main()
{
  vec3 newPos = a_Pos.xzy; // swap y and z direction to rotate plane
  gl_Position = u_ViewProjection * vec4(newPos, 1.0);
}

#type fragment
#version 410 core

out vec4 fragColor;

void main()
{
  fragColor = vec4(0.53, 0.81, 0.92, 1.0);
}