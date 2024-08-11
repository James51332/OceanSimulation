#section type(vertex)
#version 450 core

layout (location = 0) in vec3 a_Pos;
layout (location = 3) in vec2 a_UV;

layout (std140, binding = 0) uniform pushConstants
{
  mat4 u_View;
  mat4 u_Projection;
  mat4 u_ViewProjection;
  vec2 u_ViewportSize;
  float u_Time;
  float dummy;
};

layout (binding = 0) uniform sampler2D heightMap;

out vec2 v_UV;
out vec3 v_WorldPos;
out vec3 v_CameraPos;

void main()
{ 
  vec3 pos = a_Pos;
  pos.y += texture(heightMap, a_UV).r;
  gl_Position = u_ViewProjection * vec4(pos, 1.0);

  v_UV = a_UV;
  v_WorldPos = a_Pos.xyz;
  v_CameraPos = inverse(u_View)[3].xyz;
}

#section type(fragment)
#version 450 core

in vec2 v_UV;
in vec3 v_WorldPos;
in vec3 v_CameraPos;

out vec4 fragColor;

layout (binding = 0) uniform sampler2D heightMap;
layout (binding = 1) uniform sampler2D normalMap;

void main()
{
  float height = texture(heightMap, v_UV).r;
  vec3 normal = texture(normalMap, v_UV).rgb;

  vec3 lightDir = normalize(vec3(1.0, -3.0, 2.0));

  float ambient = 0.2;
  float diffuse = max(dot(normal, -lightDir), 0) * 0.8;

  vec3 camDir = normalize(v_CameraPos - v_WorldPos);
  float specular = pow(max(dot(reflect(-camDir, normal), -lightDir), 0), 16) * 0.4;

  vec3 color = vec3(0.1, 0.4, 0.9);
  float light = specular + diffuse + ambient + max(height, 0.0) * 0.2;
  fragColor = vec4(light * color, 1.0);
}
