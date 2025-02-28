#section common
#version 450 core

layout(std140, binding = 0) uniform rendererData
{
  mat4 view;                // The view matrix
  mat4 projection;          // The projection matrix
  mat4 viewProjection;      // The view projection matrix
  mat4 viewInverse;         // The inverse of the view matrix
  vec2 viewportSize;        // The size of the viewport
  float time;               // The time in seconds since the application opened
  float pushConstantsDummy; // Ensure 16-byte alignment
};

layout(std140, binding = 1) uniform wavesData
{
  // Simulation Data
  vec4 planeSize;         // The size of each simulation
  vec4 displacementScale; // The scale of each simulation's displacement

  // Rendering Data
  vec4 waveColor;        // The color of the water
  vec4 scatterColor;     // The scatter color of the water
  vec4 skyColor;         // The color of the sky
  vec4 lightColor;       // The color of the sun
  vec3 lightDirection;   // The direction toward the sun
  float sunViewAngle;    // The amount of viewspace that the sun takes up in the sky
  float sunFalloffAngle; // The fading angle between the sun and the the sky
  float fogBegin;        // The nearest position that the fog begins
  float near;            // The near camera clipping plane
  float far;             // The far camera clipping plane
};

// Data is tightly packed. Here's how: H, dHdx, dHdz, Dx, Dz, dDxdx, dDzdz, dDxdz
layout(binding = 0) uniform sampler2D heightMap[3];
layout(binding = 3) uniform sampler2D displacementMap[3];
layout(binding = 6) uniform sampler2D jacobianMap[3];

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

out vec3 v_WorldPos;
out vec3 v_CameraPos;

void main()
{
  // Our approach to tesselation is to make the grid more sparse as we increase our distance.
  // Since as distance increases, portion of eye spaces decrease inversely, we can invert our
  // distances to transform our plane into a plane with a roughly constant eye-space density.
  vec3 pos = a_Pos + vec3(15.0, 0.0, 15.0);

  // This is a funky approach to rotate the plane around the camera. As long as our FOV is less than
  // 90 degrees, we are good. However, this is pretty performance intensive and takes around 2ms on
  // my GPU. Ideally, we could use tesselation; however, I like this approach since it works on
  // metal. If we wanted to get really fancy, we could create a second camera matrix without this
  // y-axis rotation, and apply the transform on the CPU at startup.
  vec3 forward = -viewInverse[0].xyz;
  vec2 turnDir = normalize(forward.xz);
  float sqrtHalf = 0.70711;
  turnDir = vec2(turnDir.x - turnDir.y, turnDir.x + turnDir.y);
  turnDir *= sqrtHalf;
  pos.xz = vec2(turnDir.x * pos.x - turnDir.y * pos.z, pos.x * turnDir.y + pos.z * turnDir.x);

  // Scale and shift based on the position of the camera.
  vec3 cameraPos = viewInverse[3].xyz;

  // We scale based on the depth to the camera.
  pos.xz *= pow(max(length(pos.xz), 1.0), 1.2) * max(cameraPos.y, 10.0) * 0.04;

  // Center around the camera.
  pos.xz += cameraPos.xz;

  // Now we can continue as before.
  for (int i = 0; i < 3; i++)
  {
    vec2 uv = pos.xz / planeSize[i];
    vec4 data1 = texture(heightMap[i], uv);
    vec4 data2 = texture(displacementMap[i], uv);

    pos.x += displacementScale[i] * data1.w;
    pos.y += data1.x;
    pos.z += displacementScale[i] * data2.x;
  }
  gl_Position = viewProjection * vec4(pos, 1.0);

  v_WorldPos = pos.xyz;
  v_CameraPos = viewInverse[3].xyz;
}

#section type(fragment) name(waveFragment)

in vec3 v_WorldPos;
in vec3 v_CameraPos;

out vec4 FragColor;

void main()
{
  // We calculate the slope of the wave surface at each point to get normal vectors for lighting.
  vec4 d = vec4(0.0);
  float jacobian = 0.0;
  for (int i = 0; i < 3; i++)
  {
    vec2 uv = v_WorldPos.xz / planeSize[i];
    vec4 data1 = texture(heightMap[i], uv);
    vec4 data2 = texture(displacementMap[i], uv);

    jacobian += texture(jacobianMap[i], uv).r / 3.0;

    // The math for this is whacky.
    float f = displacementScale[i];
    d += vec4(data1.y, data2.y * f, data1.z, data2.z * f);
  }

  // Calculate our normal vector by black magic.
  vec2 slope = vec2(d.x / (1 + d.y), d.z / (1 + d.w));
  vec3 normal = normalize(vec3(-slope.x, 1, -slope.y));

  // Calculate the lighting information. This depends on the direction of the light (diffuse), the
  // direction of the camera (specular), and an ambient constant.
  vec3 lightDir = -normalize(lightDirection);
  vec3 camDir = normalize(v_CameraPos - v_WorldPos);
  vec3 reflectionDir = reflect(-camDir, normal);

  // Our intensity is the combination of these factors
  float ambient = 0.5;
  float diffuse = max(dot(normal, -lightDir), 0) * 0.3;
  float specular = pow(max(dot(reflectionDir, -lightDir), 0), 32) * 0.5;
  float scatter = max(v_WorldPos.y * 0.1, 0.0);
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

  // Remove the translation component from the view matrix.
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

#section type(vertex) name(postVertex)

layout(location = 0) in vec3 a_Pos;
layout(location = 3) in vec2 a_UV;

out vec2 v_UV;

void main()
{
  gl_Position = vec4(a_Pos, 1.0);
  v_UV = a_UV;
}

#section type(fragment) name(postFragment)

// We use a later binding so that we can still have the other textures declared in common section.
layout(binding = 9) uniform sampler2D colorTexture;
layout(binding = 10) uniform sampler2D depthTexture;
layout(binding = 11) uniform sampler2D skyboxColor;

in vec2 v_UV;

out vec4 FragColor;

void main()
{
  // Compute the color of the fragment by blending sky and waves based on distance.
  vec4 color = vec4(texture(colorTexture, v_UV).rgb, 1.0);
  float depth = texture(depthTexture, v_UV).r;

  // Use some math to undistort the depth buffer which is messed up by projection matrix.
  float ndc = 2.0 * depth - 1.0;
  float linearDepth = (2.0 * near * far) / (far + near - ndc * (far - near));

  // We cull the fog if it is closer than the starting point.
  float fogDensity = 0.0025;
  float fogFactor = max(1.0 - exp(-(linearDepth - fogBegin) * fogDensity), 0.0);

  FragColor = vec4(mix(color, texture(skyboxColor, v_UV), fogFactor).rgb, 1.0);
}