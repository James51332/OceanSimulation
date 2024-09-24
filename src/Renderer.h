#pragma once

#include "renderer/RenderDevice.h"
#include "renderer/Renderer.h"

namespace Waves
{

class WaveRenderer
{
  using ID = Vision::ID;

public:
  WaveRenderer(Vision::RenderDevice *renderDevice, Vision::Renderer *renderer, float width,
               float height);
  ~WaveRenderer();

  void UpdateCamera(float timestep);
  void Render(ID heightMap, ID normalMap, ID displacement);
  void Resize(float width, float height);

  void LoadShaders();

private:
  void GeneratePipelines();
  void GenerateTextures();
  void GenerateBuffers();

private:
  // General Rendering Data
  Vision::RenderDevice *renderDevice = nullptr;
  Vision::Renderer *renderer = nullptr;
  float width = 0.0f, height = 0.0f;
  Vision::PerspectiveCamera *camera = nullptr;

  // Meshes
  Vision::Mesh *planeMesh = nullptr; // surface of water
  Vision::Mesh *cubeMesh = nullptr;  // skybox and light visualization

  // Textures
  ID skyboxTexture = 0;

  // Pipelines and Shaders
  ID wavePS = 0, transparentPS = 0, skyboxPS = 0;

  // Dynamic Data and Buffers
  ID wavesBuffer = 0;
  ID lightBuffer = 0;
};

} // namespace Waves