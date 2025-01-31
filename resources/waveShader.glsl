#section type(vertex) name(waveVertex)
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

layout(binding = 0) uniform sampler2D heightMap[3];
layout(binding = 3) uniform sampler2D slopeMap[3];
layout(binding = 6) uniform sampler2D displacementMap[3];

out vec3 v_WorldPos;
out vec3 v_CameraPos;

void main()
{
  // Our approach to tesselation is to make the grid more sparse as we increase our distance.
  // Since as distance increases, portion of eye spaces decrease inversely, we can invert our
  // distances to transform our plane into a plane with a roughly constant eye-space density.
  vec3 pos = a_Pos;

  // Scale and shift based on the position of the camera.
  vec3 cameraDir = normalize(-inverse(u_View)[2].xyz);
  vec3 cameraPos = inverse(u_View)[3].xyz;

  // We scale based on the depth to the camera, but we
  float depth = abs(dot(cameraDir, vec3(pos.x, -cameraPos.y, pos.z)));
  pos.xz *= depth;

  // Center around the camera.
  pos.xz += cameraPos.xz;

  // Now we can continue as before.
  for (int i = 0; i < 3; i++)
  {
    vec2 uv = pos.xz / planeSize[i];
    pos.xz += 1.0 * texture(displacementMap[i], uv).xz;
    pos.y += texture(heightMap[i], uv).r;
  }
  gl_Position = u_ViewProjection * vec4(pos, 1.0);

  v_WorldPos = pos.xyz;
  v_CameraPos = inverse(u_View)[3].xyz;
}

#section type(fragment) name(waveFragment)
#version 450 core

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

in vec3 v_WorldPos;
in vec3 v_CameraPos;

out vec4 fragColor;

layout(binding = 0) uniform sampler2D heightMap[3];
layout(binding = 3) uniform sampler2D slopeMap[3];
layout(binding = 6) uniform sampler2D displacementMap[3];

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
  vec3 lightDir = -lightDirection;
  vec3 camDir = normalize(v_CameraPos - v_WorldPos);
  vec3 reflectionDir = reflect(-camDir, normal);

  // Our intensity is the combination of these factors
  float ambient = 0.4;
  float diffuse = max(dot(normal, -lightDir), 0) * 0.6;
  float specular = pow(max(dot(reflectionDir, -lightDir), 0), 64) * 0.6;
  float scatter = max(v_WorldPos.y * 0.4, 0.0);
  float light = diffuse + ambient + specular;

  // The color is the product of the light intensity, color at the surface, reflection color.
  vec3 color =
      light * (waveColor.rgb + scatter * vec3(0.2, 0.6, 0.9)) * SampleSkybox(reflectionDir).rgb;
  fragColor = vec4(color, 1.0);
}
