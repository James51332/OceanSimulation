#pragma once

#include "renderer/RenderDevice.h"

namespace Waves
{

// This class builds the necessary GPU data structures to perform a radix-2 Cooley-Tukey FFT on the
// GPU using compute shaders. It must be configured with a texture size upon initialization, which
// cannot be changed during the lifetime of the object.
class FFTCalculator
{
public:
  // Creates the necessary buffers and pipelines to configure and FFT for a specific size.
  FFTCalculator(Vision::RenderDevice* device, std::size_t textureSize = 512);

  // Cleans up and frees all of the objects allocated by this class.
  ~FFTCalculator();

  // Encodes the compute pass commands necessary to encode an inverse FFT. Requires that a compute
  // command encoder is already active.
  void EncodeIFFT(Vision::ID image);

  std::size_t GetTextureResolution() const { return textureSize; }

private:
  // Structure for informing GPU where in the iterative process the algorithm is.
  struct FFTPass
  {
    int passNumber = 0;
    uint32_t vertical = false;
    int totalSize = 0;
    int dummy = 0; // Uniform data has to be 16-byte aligned.
  };

private:
  // Maintain a pointer to the render device to encode the compute commands.
  Vision::RenderDevice* device;

  // Store the size of the texture as it determines the number of threagroups that we dispatch.
  std::size_t textureSize = 0;

  // Also track the number of passes since there is no need to recompute each time we encode.
  std::size_t numPasses = 0;

  // THe pipeline state which holds the compute kernels needed to encode the FFT.
  Vision::ID fftPS;

  // This is persistent memory within any given render/compute pass. To use different settings,
  // we can allocate this as an array and changed the offset between GPU calls.
  Vision::ID fftUBO = 0;

  // GPU drivers are finicky, and although we should be able to read and write to the same image
  // using threadgroup synchronization, it seems to fail to driver bugs. This approach ping-pongs
  // data between our given image and this workspace image, which sits better with the GPU, but
  // still requires GPU synchronization.
  Vision::ID workImage = 0;
};

} // namespace Waves