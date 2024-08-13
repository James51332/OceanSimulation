#pragma once

#include <glm/glm.hpp>

#include "renderer/RenderDevice.h"

namespace Waves
{

// Manages the compute shaders for our wave generation
class Generator
{
public:
  Generator(Vision::RenderDevice* device);
  ~Generator();

  void CalculateOcean(float timestep);

  struct OceanSettings
  {
    float time = 0.0f;
    float planeSize = 20.0f;
    float gravity = 9.8f;
    float scale = 0.01;
    glm::vec2 windVelocity = glm::vec2(3.0f, 5.0f);
    glm::vec2 dummy;
  };
  OceanSettings& GetOceanSettings() { return oceanSettings; }

  void LoadShaders();

  void CreateTextures();
  Vision::ID GetHeightMap() const { return heightMap; }
  Vision::ID GetNormalMap() const { return normalMapX; }
  Vision::ID GetDisplacementMap() const { return displacementX; }

private:
  Vision::RenderDevice* renderDevice = nullptr;
  Vision::ID computePS = 0;

  Vision::ID uniformBuffer = 0;
  OceanSettings oceanSettings;

  std::size_t textureSize = 1024;
  Vision::ID heightMap = 0;
  Vision::ID normalMapX = 0;
  Vision::ID normalMapZ = 0;
  Vision::ID displacementX = 0;
  Vision::ID displacementZ = 0;
  Vision::ID gaussianImage = 0;
};

}
