#pragma once

#include "renderer/RenderDevice.h"

namespace Waves
{

// Manages the compute shaders for our wave generation
class Generator
{
public:
  Generator(Vision::RenderDevice* device);
  ~Generator();

  void GenerateWaves(float timestep);
  
  Vision::ID GetHeightMap() const { return heightMap; }
  Vision::ID GetNormalMap() const { return normalMap; } 

private:
  Vision::RenderDevice* renderDevice = nullptr;

  Vision::ID heightMap = 0;
  Vision::ID normalMap = 0;

  Vision::ID computePS = 0;
};

}