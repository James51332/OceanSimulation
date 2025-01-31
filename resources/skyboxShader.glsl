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

layout(std140, binding = 1) uniform skyboxBuffer
{
  vec4 planeSize; // The size of each simulation.
  vec4 waveColor; // The color of the water.

  vec4 skyColor;         // The color of the sky.
  vec4 lightColor;       // The color of the sun.
  vec3 lightDirection;   // The direction toward the sun.
  float sunViewAngle;    // The amount of viewspace that the sun takes up in the sky.
  float sunFalloffAngle; // The fading angle between the sun and the the sky
  float cameraFOV;       // The FOV of the camera

  vec2 skyboxDummy; // Ensure we are 16-byte aligned.
};

// Extract this to a function so we can copy over to waveShader to reflect the sun and not just the
// skybox.
vec4 SampleSkybox(vec3 direction)
{
  // Calculate the sun by taking the dot product with the texCoord.
  float sunSkyCosine = dot(normalize(lightDirection), normalize(direction));
  float cosineThreshold = cos(sunViewAngle * 3.1415 / 180.0);
  float fadeThreshold = cos((sunViewAngle + sunFalloffAngle) * 3.1415 / 180.0);

  // We should claculate the sun's influence based on weather or not the dot product is above
  // our threshold. Use a smoothstep here to blend between where the sun is and where it is not.
  float sunInfluence = smoothstep(fadeThreshold, cosineThreshold, sunSkyCosine);

  // Now square this value to create a more rapid falloff.
  sunInfluence *= sunInfluence * sunInfluence;

  // This is our true skybox color
  vec4 skyboxColor = mix(skyColor, lightColor, pow(0.8 - normalize(direction).y / 0.8, 2));

  // Blend the sun with the skybox color.
  // We could just interpolate, but this looks bad. Instead, we can portray the atmosphere as
  // slightly in front of the son using a blend mode.
  return (1 - sunInfluence) * skyboxColor + 1.6 * sunInfluence * lightColor;
}

void main()
{
  FragColor = SampleSkybox(texCoord);
}