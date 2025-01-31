#section common
#version 450 core

layout(std140, binding = 0) uniform rendererData
{
  mat4 view;                // The view matrix
  mat4 projection;          // The projection matrix
  mat4 viewProjection;      // The view projection matrix
  vec2 viewportSize;        // The size of the viewport
  float time;               // The time in seconds since the application opened
  float pushConstantsDummy; // Ensure 16-byte alignment
};

layout(std140, binding = 1) uniform wavesData
{
  // Simulation Data
  vec4 planeSize;                       // The size of each simulation
  float displacementScale;              // The scale of the displacment of the water
  float disDummy1, diDummy2, disDummy3; // Align to 16 byte data before switching data types.

  // Rendering Data
  vec4 waveColor;        // The color of the water
  vec4 scatterColor;     // The scatter color of the water
  vec4 skyColor;         // The color of the sky
  vec4 lightColor;       // The color of the sun
  vec3 lightDirection;   // The direction toward the sun
  float sunViewAngle;    // The amount of viewspace that the sun takes up in the sky
  float sunFalloffAngle; // The fading angle between the sun and the the sky
  float cameraFOV;       // The FOV of the camera
  vec2 skyboxDummy;      // Ensure we are 16-byte aligned
};

layout(binding = 0) uniform sampler2D heightMap[3];
layout(binding = 3) uniform sampler2D slopeMap[3];
layout(binding = 6) uniform sampler2D displacementMap[3];

#define DEGREE_TO_RADIANS 0.0174533

vec4 SampleSkybox(vec3 direction)
{
  // Normalize the direction so that it doesn't scale our dot products.
  direction = normalize(direction);

  // Calculate the sun by taking the dot product with the texCoord.
  float sunSkyCosine = dot(normalize(lightDirection), direction);
  float cosineThreshold = cos(sunViewAngle * DEGREE_TO_RADIANS);
  float fadeThreshold = cos((sunViewAngle + max(sunFalloffAngle, 0.01)) * DEGREE_TO_RADIANS);

  // We should claculate the sun's influence based on weather or not the dot product is above
  // our threshold. Use a smoothstep here to blend between where the sun is and where it is not.
  float sunInfluence = smoothstep(fadeThreshold, cosineThreshold, sunSkyCosine);

  // Now square this value to create a more rapid falloff.
  sunInfluence *= sunInfluence * sunInfluence;

  // This is our true skybox color
  vec4 skyboxColor = mix(skyColor, lightColor, pow(0.8 - direction.y / 0.8, 2));

  // Blend the sun with the skybox color, burning the sun past max to max white
  return (1.0 - sunInfluence) * skyboxColor + (2.0 * sunInfluence) * lightColor;
}

#section type(vertex) name(waveVertex)

layout(location = 0) in vec3 a_Pos;
layout(location = 3) in vec2 a_UV;

out vec3 v_WorldPos;
out vec3 v_CameraPos;

void main()
{
  // Our approach to tesselation is to make the grid more sparse as we increase our distance.
  // Since as distance increases, portion of eye spaces decrease inversely, we can invert our
  // distances to transform our plane into a plane with a roughly constant eye-space density.
  vec3 pos = a_Pos;

  // Scale and shift based on the position of the camera.
  vec3 cameraDir = normalize(-inverse(view)[2].xyz);
  vec3 cameraPos = inverse(view)[3].xyz;

  // We scale based on the depth to the camera.
  float depth = abs(dot(cameraDir, vec3(pos.x, -cameraPos.y, pos.z)));
  pos.xz *= depth;

  // Center around the camera.
  pos.xz += cameraPos.xz;

  // Now we can continue as before.
  for (int i = 0; i < 3; i++)
  {
    vec2 uv = pos.xz / planeSize[i];
    pos.xz += displacementScale * texture(displacementMap[i], uv).xz;
    pos.y += texture(heightMap[i], uv).r;
  }
  gl_Position = viewProjection * vec4(pos, 1.0);

  v_WorldPos = pos.xyz;
  v_CameraPos = inverse(view)[3].xyz;
}

#section type(fragment) name(waveFragment)

in vec3 v_WorldPos;
in vec3 v_CameraPos;

out vec4 FragColor;

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
  float scatter = max(v_WorldPos.y * 0.15, 0.0);
  float light = diffuse + ambient + specular;

  // The color is the product of the light intensity, color at the surface, reflection color.
  vec3 color = light * waveColor.rgb * SampleSkybox(reflectionDir).rgb + scatter * scatterColor.rgb;
  FragColor = vec4(color, 1.0);
}

#section type(vertex) name(skyVertex)

layout(location = 0) in vec3 a_Pos;

out vec3 texCoord;

void main()
{
  // Send the texture coordinate to the fragment shader to sample skybox.
  texCoord = a_Pos;

  // Remove the translation component from the view matrix
  mat4 noTranslateView = mat4(mat3(view));

  // Project and place the depth in normalized coords as far as possible.
  vec4 pos = projection * noTranslateView * vec4(a_Pos, 1.0);
  gl_Position = pos.xyww;
}

#section type(fragment) name(skyFragment)

in vec3 texCoord;

out vec4 FragColor;

void main()
{
  FragColor = SampleSkybox(texCoord);
}