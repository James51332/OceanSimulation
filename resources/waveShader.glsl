#type vertex
#version 410 core

layout (location = 0) in vec3 a_Pos;

struct Wave
{
  vec2 origin;
  vec2 direction;
  vec4 scale; // amplitude, wavelength, angular freq., phase 
};

uniform mat4 u_ViewProjection;
uniform float u_Time;

layout (std140) uniform WaveProperties
{
  Wave waves[10];
}; 

out vec3 normal;

void main()
{
  vec3 pos = a_Pos.xzy; // swap y and z direction to rotate plane
  
  // store the partial derivatives for the x directions and the z directions.
  float partialX = 0;
  float partialZ = 0;

  for (int i = 0; i < 1; i++)
  {
    Wave wave = waves[i];

    // calculate the distance with respect to the direction of the wave.
    vec2 displacement = pos.xz - wave.origin;
    float dist = dot(displacement, wave.direction);

    // determine the height offset using general sinusoid equation
    float waveNumber = 6.283185307 / wave.scale.y; // wave number = 2 * PI / wavelength
    float waveInput = waveNumber * dist - wave.scale.z * u_Time + wave.scale.w; // store this
    float heightOffset = wave.scale.x * sin(waveInput);
    pos.y += heightOffset;

    // calculate the partial derivatives in x and z directions
    partialX += wave.scale.x * cos(waveInput) * wave.direction.x * waveNumber;
    partialZ += wave.scale.x * cos(waveInput) * wave.direction.y * waveNumber;
  }

  // calculate the normal vector
  vec3 binormal = normalize(vec3(1, 0, partialX));
  vec3 tangent = normalize(vec3(0, 1, partialZ));
  normal = cross(binormal, tangent);
  
  // project from 3D space to 2D
  gl_Position = u_ViewProjection * vec4(pos, 1.0);
}

#type fragment
#version 410 core

in vec3 normal;

out vec4 fragColor;

void main()
{
  vec3 lightDir = normalize(vec3(2.0, 1.0, -3.0));

  float diffuse = clamp(dot(normal, -lightDir), 0, 1) * 0.8;
  float ambient = 0.4;

  vec3 color = vec3(0.53, 0.81, 0.92);
  fragColor = vec4(color * (ambient + diffuse), 1.0);
}