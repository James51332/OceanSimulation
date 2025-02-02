#pragma once

#include <vector>

#include "renderer/RenderDevice.h"
#include "renderer/Renderer.h"

#include "Generator.h"

namespace Waves
{

// Wave Uniform Buffer Data Structure
struct WaveRenderData
{
  // Simulation Data
  glm::vec4 planeSize = glm::vec4(0.0f); // The size of the three planes that make up our water
  float displacementScale = 0.1f;        // The scale of the displacment of the water
  float dDummy1, dDummy2, dDummy3;       // Align to 16 byte data before switching data types

  // Rendering Data
  glm::vec4 waveColor = glm::vec4(1.0f);                  // The color of the wave
  glm::vec4 scatterColor = glm::vec4(1.0f);               // Scatter color of the water
  glm::vec4 skyColor = glm::vec4(0.0f, 0.5f, 1.0f, 1.0f); // Color of the sky
  glm::vec4 sunColor = glm::vec4(1.0f);                   // Color of sun and horizon
  glm::vec3 lightDirection = glm::vec3(0.0f, 1.0f, 0.0f); // Direction towards the sun
  float sunViewAngle = 2.0f;                              // The angle of the sun in the sky
  float sunFalloffAngle = 0.0f;                           // The angle between hard edge and sky

  // Ensure that we are 16-byte aligned.
  glm::vec3 dummy;
};

class WaveRenderer
{
  using ID = Vision::ID;

public:
  WaveRenderer(Vision::RenderDevice* renderDevice, Vision::Renderer* renderer, float width,
               float height);
  ~WaveRenderer();

  void UpdateCamera(float timestep);

  // Requires that the generators is an array of three valid FFT oceans
  void Render(std::vector<Generator*>& generators);
  static std::size_t GetNumRequiredGenerators() { return 3; }

  void Resize(float width, float height);

  void UseWireframe(bool wireframe = true) { useWireframe = wireframe; }
  void ToggleWireframe() { useWireframe = !useWireframe; }
  bool UsesWireframe() const { return useWireframe; }

  void LoadShaders();

  WaveRenderData& GetWaveRenderData() { return wavesBufferData; }

private:
  void GeneratePasses();
  void GeneratePipelines();
  void GenerateBuffers();

private:
  // General Rendering Data
  Vision::RenderDevice* renderDevice = nullptr;
  Vision::Renderer* renderer = nullptr;
  float width = 0.0f, height = 0.0f;
  Vision::PerspectiveCamera* camera = nullptr;
  ID wavePass = 0, postPass = 0;

  // Rendering Framebuffer (used for post processing)
  ID framebuffer = 0, fbColor = 0;
  ID postPS = 0;

  // Meshes
  Vision::Mesh* planeMesh = nullptr; // surface of water
  Vision::Mesh* cubeMesh = nullptr;  // skybox and light visualization
  Vision::Mesh* quadMesh = nullptr;  // quad that covers screen for rendering fb

  // Pipelines and Shaders
  bool useWireframe = false;
  ID wavePS = 0, wireframePS = 0, skyboxPS = 0;

  // Wave Uniform Buffer
  WaveRenderData wavesBufferData;
  ID wavesBuffer = 0;
};

} // namespace Waves