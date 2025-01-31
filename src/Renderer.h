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
  // Wave Simulation Data
  float planeSize[4];  // The size of the three planes that make up our water.
  glm::vec4 waveColor; // The color of the wave.

  // Skybox Data
  glm::vec4 scatterColor;
  glm::vec4 skyColor;
  glm::vec4 sunColor;
  glm::vec3 lightDirection;
  float sunViewAngle;
  float sunFalloffAngle;
  float cameraFOV;

  // Ensure that we are 16-byte aligned.
  glm::vec2 dummy;
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

  void LoadShaders();

  WaveRenderData& GetWaveRenderData() { return wavesBufferData; }

private:
  void GeneratePipelines();
  void GenerateBuffers();

private:
  // General Rendering Data
  Vision::RenderDevice* renderDevice = nullptr;
  Vision::Renderer* renderer = nullptr;
  float width = 0.0f, height = 0.0f;
  Vision::PerspectiveCamera* camera = nullptr;

  // Meshes
  Vision::Mesh* planeMesh = nullptr; // surface of water
  Vision::Mesh* cubeMesh = nullptr;  // skybox and light visualization

  // Pipelines and Shaders
  ID wavePS = 0, transparentPS = 0, skyboxPS = 0;

  // Wave Uniform Buffer
  WaveRenderData wavesBufferData;
  ID wavesBuffer = 0;
};

} // namespace Waves