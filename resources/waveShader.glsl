#section type(vertex)
#version 450 core

layout(location = 0) in vec3 a_Pos;
layout(location = 3) in vec2 a_UV;

layout(std140, binding = 0) uniform pushConstants
{
  mat4 u_View;
  mat4 u_Projection;
  mat4 u_ViewProjection;
  vec2 u_ViewportSize;
  float u_Time;
  float dummy;
};

layout(std140, binding = 1) uniform waveShaderConstants
{
  // The color of the wave.
  vec4 waveColor;

  // The position of the light in the sky. We can make this very far away.
  vec3 lightPos;
  float lightDummy;

  // The size of the three planes that make up our water.
  vec4 planeSize;
};

layout(binding = 0) uniform sampler2D heightMap[3];
layout(binding = 3) uniform sampler2D slopeMap[3];
layout(binding = 6) uniform sampler2D displacementMap[3];
layout(binding = 9) uniform samplerCube skybox;

out vec3 v_WorldPos;
out vec3 v_CameraPos;

void main()
{
  // Our approach to tesselation is to make the grid more sparse as we increase our distance.
  // Since as distance increases, portion of eye spaces decrease inversely, we can invert our
  // distances to transform our plane into a plane with a roughly constant eye-space density.
  vec3 pos = a_Pos;

  // Use our adjusted formula (separates inside points from outside points)
  float linearScalar = 0.1;
  float growthFactor = 0.2;
  vec2 powFactor = vec2(pow(1 + growthFactor, abs(pos.x)), pow(1 + growthFactor, abs(pos.z)));
  pos.xz = pos.xz * powFactor * linearScalar;

  // Scale and shift based on the position of the camera.
  vec3 cameraPos = inverse(u_View)[3].xyz;
  pos.xz *= max(cameraPos.y, 0.5);
  pos.xz += cameraPos.xz;

  // Now we can continue as before.
  for (int i = 0; i < 3; i++)
  {
    vec2 uv = pos.xz / planeSize[i];
    pos.xz += texture(displacementMap[i], uv).xz;
    pos.y += texture(heightMap[i], uv).r;
  }
  gl_Position = u_ViewProjection * vec4(pos, 1.0);

  v_WorldPos = pos.xyz;
  v_CameraPos = inverse(u_View)[3].xyz;
}

#section type(fragment)
#version 450 core

layout(std140, binding = 1) uniform waveShaderConstants
{
  // The color of the wave.
  vec4 waveColor;

  // The position of the light in the sky. We can make this very far away.
  vec3 lightPos;
  float dummy2;

  // The size of the three planes that make up our water.
  vec4 planeSize;
};

in vec3 v_WorldPos;
in vec3 v_CameraPos;

out vec4 fragColor;

layout(binding = 0) uniform sampler2D heightMap[3];
layout(binding = 3) uniform sampler2D slopeMap[3];
layout(binding = 3) uniform sampler2D displacementMap[3];
layout(binding = 9) uniform samplerCube skybox;

void main()
{
  // We calculate the slope of the wave surface at each point to get normal vectors for lighting.
  vec3 slope = vec3(0.0);
  for (int i = 0; i < 3; i++)
  {
    vec2 uv = v_WorldPos.xz / planeSize[i];
    slope += texture(slopeMap[i], uv).rgb;
  }

  // Calculate our normal vector by crossing the binormal and tangent vector.
  vec3 binormal = vec3(1.0, slope.x, 0.0);
  vec3 tangent = vec3(0.0, slope.z, 1.0);
  vec3 normal = normalize(cross(tangent, binormal));

  // Calculate the lighting information. This depends on the direction of the light (diffuse), the
  // direction of the camera (specular), and an ambient constant.
  vec3 lightDir = normalize(v_WorldPos - lightPos);
  vec3 camDir = normalize(v_CameraPos - v_WorldPos);
  vec3 reflectionDir = reflect(-camDir, normal);

  // Our intensity is the combination of these factors
  float ambient = 0.6;
  float diffuse = max(dot(normal, -lightDir), 0) * 0.6;
  float specular = pow(max(dot(reflectionDir, -lightDir), 0), 64) * 0.6;
  float light = diffuse + ambient + specular;

  // The color is the product of the light intensity, color at the surface, reflection color.
  vec3 color = light * waveColor.rgb * texture(skybox, reflectionDir).rgb;
  fragColor = vec4(color, 1.0);
}
