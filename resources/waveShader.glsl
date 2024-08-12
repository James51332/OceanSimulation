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
layout (binding = 3) uniform sampler2D displacement;

out vec2 v_UV;
out vec3 v_WorldPos;
out vec3 v_CameraPos;

void main()
{ 
  vec3 pos = a_Pos;
  pos.xz += texture(displacement, a_UV).xz;
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
layout (binding = 2) uniform samplerCube skybox;

void main()
{
  float height = texture(heightMap, v_UV).r;
  vec3 normal = texture(normalMap, v_UV).rgb;

  vec3 lightDir = normalize(vec3(5.0, -3.0, 2.0));

  float ambient = 0.4;
  float diffuse = max(dot(normal, -lightDir), 0) * 0.5;

  vec3 camDir = normalize(v_CameraPos - v_WorldPos);
  vec3 reflectionDir = reflect(-camDir, normal);

  float specular = pow(max(dot(reflectionDir, -lightDir), 0), 64) * 0.6;

  vec3 color = vec3(0.53, 0.81, 0.92) * texture(skybox, reflectionDir).rgb;
  float light = diffuse + ambient + specular + max(height, 0.0) * 0.2;
  fragColor = vec4(light * color, 1.0);
}
