#type vertex
#version 450 core

layout (location = 0) in vec3 a_Pos;

layout (binding = 0) uniform pushConstants
{
  mat4 u_View;
  mat4 u_Projection;
  mat4 u_ViewProjection;
  vec2 u_ViewportSize;
  float u_Time;
  float dummy; // 16 byte alignment
};

struct Wave
{
  vec2 origin;
  vec2 direction;

  float amplitude; 
  float wavelength;
  float angularFrequency;
  float phase;
};

layout (binding = 1) uniform WaveProperties
{
  Wave waves[10];
};

layout (binding = 2) uniform Time
{
  float time;
};

out vec3 worldPos;
out vec3 normal;

void main()
{
  vec4 pos = vec4(a_Pos, 1.0);

  // store the partial derivatives for the x directions and the z directions.
  float partialX = 0;
  float partialZ = 0;

  for (int i = 0; i < 10; i++)
  {
    Wave wave = waves[i];

    // calculate the distance with respect to the direction of the wave.
    vec2 displacement = pos.xz - wave.origin;
    float dist = dot(displacement, wave.direction);

    // determine the height offset using general sinusoid equation
    float waveNumber = 6.283185307 / wave.wavelength; // wave number = 2 * PI / wavelength
    float waveInput = waveNumber * dist - wave.angularFrequency * u_Time + wave.phase; // store this
    float heightOffset = wave.amplitude * sin(waveInput);
    pos.y += heightOffset;

    // calculate the partial derivatives in x and z directions
    partialX += wave.amplitude * cos(waveInput) * wave.direction.x * waveNumber;
    partialZ += wave.amplitude * cos(waveInput) * wave.direction.y * waveNumber;
  }

  //calculate the normal vector
  vec3 binormal = normalize(vec3(1, 0, partialX));
  vec3 tangent = normalize(vec3(0, 1, partialZ));
  normal = cross(binormal, tangent);

  //transform to clip space
  gl_Position = u_ViewProjection * pos;
  worldPos = pos.xyz;
}

#type fragment
#version 450 core

in vec3 worldPos;
in vec3 normal;

out vec4 fragColor;

layout (binding = 3) uniform LightProperties
{
  vec4 lightPos;
  vec4 cameraPos;
  vec4 waveColor;
};

void main()
{
  // choose some arbitary light direction
  vec3 lightDir = normalize(worldPos - lightPos.xyz);

  // diffuse calculation
  float diffuseStrength = 0.5;
  float diffuse = clamp(dot(normal, -lightDir), 0, 1) * diffuseStrength;

  // specular calculation
  float specularStrength = 0.5;
  vec3 camDir = normalize(cameraPos.xyz - worldPos);
  float specular = pow(max(dot(reflect(camDir, normal), -lightDir), 0), 32) * specularStrength;

  // ambient (constant)
  float ambient = 0.6;

  fragColor = vec4(waveColor.rgb * (ambient + diffuse + specular), 1.0);
}