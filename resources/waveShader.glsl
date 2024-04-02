#type vertex
#version 410 core

layout (location = 0) in vec3 a_Pos;

struct Wave
{
  vec2 origin;
  vec2 direction;

  float amplitude; 
  float wavelength;
  float angularFrequency;
  float phase;
};

uniform mat4 u_ViewProjection;
uniform float u_Time;

layout (std140) uniform WaveProperties
{
  Wave waves[10];
}; 

out vec3 worldPos;
out vec3 normal;

void main()
{
  vec3 pos = a_Pos.xzy; // swap y and z direction to rotate plane
  
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

  // calculate the normal vector
  vec3 binormal = normalize(vec3(1, 0, partialX));
  vec3 tangent = normalize(vec3(0, 1, partialZ));
  normal = cross(binormal, tangent);
  
  // send the world position to the fragment shader
  worldPos = pos;

  // project from 3D space to 2D
  gl_Position = u_ViewProjection * vec4(pos, 1.0);
}

#type fragment
#version 410 core

in vec3 worldPos;
in vec3 normal;

uniform vec3 u_CameraPos;

out vec4 fragColor;

void main()
{
  // choose some arbitary light direction
  vec3 lightDir = normalize(vec3(1.0, -0.2, 0.0));

  // diffuse calculation
  float diffuseStrength = 0.4;
  float diffuse = clamp(dot(normal, -lightDir), 0, 1) * diffuseStrength;

  // specular calculation
  float specularStrength = 0.5;
  vec3 camDir = normalize(u_CameraPos - worldPos);
  float specular = pow(max(dot(reflect(camDir, normal), -lightDir), 0), 32) * specularStrength;

  // ambient (constant)
  float ambient = 0.3;

  vec3 color = vec3(0.4, 0.6, 0.81);
  fragColor = vec4(color * (ambient + diffuse + specular), 1.0);
}