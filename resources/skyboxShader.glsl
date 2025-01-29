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

layout(binding = 1) uniform skyboxBuffer
{
  vec3 lightDirection; // The direction toward the sun
  float sunViewAngle;  // The amount of viewspace that the sun takes up in the sky.
  vec4 lightColor;     // The color of the sun.
};

// Extract this to a function so we can copy over to waveShader to reflect the sun and not just the
// skybox.
vec4 SampleSkybox(vec3 direction)
{
  // Calculate the sun by taking the dot product with the texCoord.
  float sunSkyCosine = dot(lightDirection, normalize(direction));
  float cosineThreshold = cos(sunViewAngle);

  // We should claculate the sun's influence based on weather or not the dot product is above our
  // threshold. Use a smoothstep here to blend between where the sun is and where it is not.
  float sunInfluence = smoothstep(cosineThreshold - 0.1, cosineThreshold, sunSkyCosine);

  // Now square this value to create a more rapid falloff.
  sunInfluence *= sunInfluence;

  // Blend this with the skybox color.
  vec4 skyboxColor = texture(skybox, direction);
  return mix(skyboxColor, lightColor, sunInfluence);
}

void main()
{
  FragColor = SampleSkybox(texCoord);
}