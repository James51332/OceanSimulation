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

  void GenerateSpectrum();
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
  void PerformFFT(Vision::ID image);

private:
  Vision::RenderDevice* renderDevice = nullptr;
  Vision::ID computePS = 0;

  Vision::ID oceanUBO = 0;
  OceanSettings oceanSettings;

  // Since our UBOs are persistent memory, and their
  // is not a great API to set tmp memory (e.g. pushConstants,
  // glUniform, or setBytes) in Vision, We'll take the approach of
  // shifting our offset in the FFT UBO for each shader invocation
  // This also means that the memory doesn't need to change, and
  // we can fill our UBO at load-time.
  struct FFTPass
  {
    int passNumber = 0;
    uint vertical = false;
    int totalSize = 0;
    int dummy = 0;
  };
  Vision::ID fftUBO = 0;

  std::size_t textureSize = 512;
  Vision::ID heightMap = 0;
  Vision::ID normalMapX = 0;
  Vision::ID normalMapZ = 0;
  Vision::ID displacementX = 0;
  Vision::ID displacementZ = 0;
  Vision::ID gaussianImage = 0;
  Vision::ID tempImage = 0;
};

}
