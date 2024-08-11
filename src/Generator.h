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

  void GenerateSpectrum(float timestep);
  void GenerateWaves();

  void LoadShaders();
  void CreateTextures();
  
  Vision::ID GetHeightMap() const { return heightMap; }
  Vision::ID GetNormalMap() const { return normalMapX; } 

private:
  Vision::RenderDevice* renderDevice = nullptr;

  Vision::ID heightMap = 0;
  Vision::ID normalMapX = 0;
  Vision::ID normalMapZ = 0;
  Vision::ID gaussianImage = 0;

  Vision::ID uniformBuffer = 0;
  float time = 0.0f;

  Vision::ID computePS = 0;

  std::size_t textureSize = 512;
};

}
