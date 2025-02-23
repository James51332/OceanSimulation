#pragma once

#include <glm/glm.hpp>

#include "renderer/RenderDevice.h"

#include "FFTCalculator.h"

namespace Waves
{

struct GeneratorSettings
{
  glm::ivec2 seed = glm::ivec2(12342, 8934); // The seed for random generation.

  float U_10 = 40.0f;         // The speed of the wind.
  float theta_0 = 25.0f;      // The CCW direction of the wind rel. to +x-axis.
  float F = 800000.0f;        // The distance to a downwind shore (fetch).
  float g = 9.8f;             // The acceleration due to gravity.
  float swell = 0.5f;         // The factor of non-wind based waves.
  float h = 100.0f;           // The depth of the ocean.
  float displacement = 0.4f;  // The scalar used in displacing the vertices.
  float time = 0.0f;          // The time in seconds since the program began.
  float planeSize = 40.0f;    // The size of the plane in meters that this plane is simulating.
  float scale = 1.0f;         // The global heightmap scalar.
  float spread = 0.2f;        // The intensity of waves perp. to wind.
  int boundWavelength = 0;    // Whether or not we bound the wavelength (1 = bound, 0 = unbound)
  float wavelengthMin = 0.0f; // The minimum wavelength that is allowed
  float wavelengthMax = 0.0f; // The maximum wavelength that is allowed
};

// Manages the compute shaders for our wave generation
class Generator
{
public:
  Generator(Vision::RenderDevice* device, FFTCalculator* calc);
  ~Generator();

  // Access the settings behind this ocean. If the spectrum is modified, then the next call to
  // calculate ocean should set updateOcean to true.
  GeneratorSettings& GetOceanSettings() { return oceanSettings; }

  // Perform the necessary FFTs to calculate the change the ocean given a timestep since the last
  // call.
  void CalculateOcean(float timestep, bool updateOcean = false);

  // Getter for the two textures used by wave shader to render.
  Vision::ID GetHeightMap() const { return heightMap; }
  Vision::ID GetDisplacementMap() const { return displacementMap; }
  Vision::ID GetJacobianMap() const { return jacobian; }

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
  GeneratorSettings oceanSettings;
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
