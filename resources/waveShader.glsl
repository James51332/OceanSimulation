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

layout(binding = 0) uniform sampler2D heightMap[3];
layout(binding = 3) uniform sampler2D slopeMap[3];
layout(binding = 6) uniform sampler2D displacementMap[3];
layout(binding = 9) uniform samplerCube skybox;

out vec2 v_UV;
out vec3 v_WorldPos;
out vec3 v_CameraPos;

void main()
{
  // Our approach to tesselation is to make the grid more sparse as we increase our distance.
  // Since as distance increases, portion of eye spaces decrease inversely, we can invert our
  // distances to transform our plane into a plane with a roughly constant eye-space density.
  vec3 pos = a_Pos;

  // Prepare our values for the inversion.
  float maxDistance = 50.0; // Size of our plane per meter of camera height.
  float planeSize = 40.0;   // The size of the initial plane mesh. (Could make this one)

  // Use our adjusted formula (separates inside points from outside points)
  float linearScalar = 0.2;
  float growthFactor = 0.2;
  vec2 powFactor = vec2(pow(1 + growthFactor, abs(pos.x)), pow(1 + growthFactor, abs(pos.z)));
  pos.xz = pos.xz * powFactor * linearScalar;

  // Scale and shift based on the position of the camera.
  vec3 cameraPos = inverse(u_View)[3].xyz;
  pos.xz *= max(cameraPos.y, 0.5);
  pos.xz += cameraPos.xz;

  // We have to adjust for our changed coordinates since our UV values are now useless.
  float textureSize = 40.0;       // TODO: Use a uniform (right now we have 20m x 20m plane)
  vec2 uv = pos.xz / textureSize; // Tile after 20m and center around origin

  // Now we can continue as before.
  for (int i = 0; i < 3; i++)
  {
    pos.xz += texture(displacementMap[i], uv).xz;
    pos.y += texture(heightMap[i], uv).r;
  }
  gl_Position = u_ViewProjection * vec4(pos, 1.0);

  v_UV = uv;
  v_WorldPos = pos.xyz;
  v_CameraPos = inverse(u_View)[3].xyz;
}

#section type(fragment)
#version 450 core

in vec2 v_UV;
in vec3 v_WorldPos;
in vec3 v_CameraPos;

out vec4 fragColor;

layout(binding = 0) uniform sampler2D heightMap[3];
layout(binding = 3) uniform sampler2D slopeMap[3];
layout(binding = 3) uniform sampler2D displacementMap[3];
layout(binding = 9) uniform samplerCube skybox;

void main()
{
  float height = 0.0;
  vec3 slope = vec3(0.0);
  for (int i = 0; i < 3; i++)
  {
    height += texture(heightMap[i], v_UV).r;
    slope += texture(slopeMap[i], v_UV).rgb;
  }

  // Calculate our normal vector
  // Get the gradient in each direction and construct binormal and tangent vectors
  vec3 binormal = vec3(1.0, slope.x, 0.0);
  vec3 tangent = vec3(0.0, slope.z, 1.0);
  vec3 normal = normalize(cross(tangent, binormal));

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
