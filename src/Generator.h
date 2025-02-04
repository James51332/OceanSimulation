#pragma once

#include <glm/glm.hpp>

#include "renderer/RenderDevice.h"

#include "FFTCalculator.h"

namespace Waves
{

struct OceanSettings
{
  float time = 0.0f;
  float planeSize = 20.0f;
  float gravity = 9.8f;
  float scale = 1.0;
  glm::vec2 windVelocity = glm::vec2(8.0f, 5.0f);
  float wavelengthMin = 0.0f;
  float wavelengthMax = 0.0f;
  float displacement = 0.0f;
  int boundWavelength = 0;
  float dummy1, dummy2;
};

// Manages the compute shaders for our wave generation
class Generator
{
public:
  Generator(Vision::RenderDevice* device, FFTCalculator* calc);
  ~Generator();

  // Access the settings behind this ocean. If the spectrum is modified, then the next call to
  // calculate ocean should set updateOcean to true.
  OceanSettings& GetOceanSettings() { return oceanSettings; }

  // Perform the necessary FFTs to calculate the change the ocean given a timestep since the last
  // call.
  void CalculateOcean(float timestep, bool updateOcean = false);

  // Getter for the two textures used by wave shader to render.
  Vision::ID GetHeightMap() const { return heightMap; }
  Vision::ID GetDisplacementMap() const { return displacementMap; }

  // Reload the shaders that are used by this class.
  void LoadShaders(bool reload = false);

private:
  void GenerateNoise();
  void GenerateTextures();
  void GenerateSpectrum();

private:
  Vision::RenderDevice* renderDevice = nullptr;
  FFTCalculator* fftCalc = nullptr;

  // The size of all textures owned by this generator.
  std::size_t textureSize;

  // Store a static pipeline state so we don't have to recreate pipelines for all.
  static inline bool generatedPS = false;
  static inline Vision::ID computePS = 0;

  // Store the settings for our ocean.
  bool updateSpectrum = true;
  OceanSettings oceanSettings;
  Vision::ID oceanUBO = 0;

  // h, dh/dx, dh/dz, Dx
  Vision::ID heightMap = 0;

  // Dz, dDx/dx, dDz/dz, dDx/dz
  Vision::ID displacementMap = 0;

  // A randomly generated image on the CPU.
  Vision::ID gaussianImage = 0;

  // Store our generated spectrum which we propogate each frame.
  Vision::ID initialSpectrum = 0;

  // Evaluate the jacobian of displacement at each point to determine where the wave curls in on
  // itself. At these point, we accumulate foam into a texture. This foam decays over time
  // exponentially. Since each simulation has its own tiling jacobian, it makes more sense to store
  // this texture in the generator.
  Vision::ID jacobian = 0;
};

} // namespace Waves
