#pragma once

#include <vector>

#include "renderer/RenderDevice.h"
#include "renderer/Renderer.h"

#include "Generator.h"

namespace Waves
{

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

private:
  void GeneratePipelines();
  void GenerateTextures();
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

  // Textures
  ID skyboxTexture = 0;

  // Pipelines and Shaders
  ID wavePS = 0, transparentPS = 0, skyboxPS = 0;

  // Dynamic Data and Buffers
  struct WaveBuffer
  {
    glm::vec4 waveColor;      // The color of the wave.
    glm::vec3 lightDirection; // The direction towards the sun.
    float lightDirDummy;      // Dummy for the light direction.
    float planeSize[4];       // The size of the three planes that make up our water.
  };
  WaveBuffer wavesBufferData;
  ID wavesBuffer = 0;

  struct SkyboxBuffer
  {
    glm::vec3 lightDirection;
    float lightDirDummy;
  };
  SkyboxBuffer skyboxBufferData;
  ID skyboxBuffer = 0;
};

} // namespace Waves