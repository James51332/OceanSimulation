#pragma once

#include <glm/glm.hpp>

#include "renderer/RenderDevice.h"

#include "FFTCalculator.h"

namespace Waves
{

// Manages the compute shaders for our wave generation
class Generator
{
public:
  Generator(Vision::RenderDevice* device, FFTCalculator* calc);
  ~Generator();

  void CalculateOcean(float timestep, bool updateOcean = false);

  struct OceanSettings
  {
    float time = 0.0f;
    float planeSize = 20.0f;
    float gravity = 9.8f;
    float scale = 1.0;
    glm::vec2 windVelocity = glm::vec2(8.0f, 5.0f);
    float wavelengthMin = 0.0f;
    float wavelengthMax = 0.0f;
    int boundWavelength = 0;
    int dummy1, dummy2, dummy3;
  };
  OceanSettings& GetOceanSettings() { return oceanSettings; }

  void LoadShaders();

  void CreateTextures();
  Vision::ID GetHeightMap() const { return heightMap; }
  Vision::ID GetSlopeMap() const { return slopeMap; }
  Vision::ID GetDisplacementMap() const { return displacementMap; }

private:
  void GenerateNoise();
  void GenerateSpectrum();

private:
  Vision::RenderDevice* renderDevice = nullptr;
  FFTCalculator* fftCalc = nullptr;

  static inline bool generatedPS = false;
  static inline Vision::ID computePS = 0;

  Vision::ID oceanUBO = 0;
  OceanSettings oceanSettings;
  bool updateSpectrum = true;

  std::size_t textureSize;
  Vision::ID heightMap = 0;
  Vision::ID slopeMap = 0;
  Vision::ID displacementMap = 0;
  Vision::ID gaussianImage = 0;
  Vision::ID initialSpectrum = 0;
};

} // namespace Waves
