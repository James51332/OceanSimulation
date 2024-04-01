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
  Wave wave;
} waves; 

void main()
{
  Wave wave = waves.wave;
  vec3 pos = a_Pos.xzy; // swap y and z direction to rotate plane

  // calculate the distance with respect to the direction of the wave.
  vec2 displacement = pos.xz - wave.origin;
  float dist = dot(displacement, wave.direction);

  // determine the height offset using general sinusoid equation
  float waveNumber = 6.283185307 / wave.scale.y; // wave number = 2 * PI / wavelength
  float heightOffset = wave.scale.x * sin(waveNumber * dist - wave.scale.z * u_Time + wave.scale.w);
  pos.y += heightOffset;
  
  // project from 3D space to 2D
  gl_Position = u_ViewProjection * vec4(pos, 1.0);
}

#type fragment
#version 410 core

out vec4 fragColor;

void main()
{
  fragColor = vec4(0.53, 0.81, 0.92, 1.0);
}