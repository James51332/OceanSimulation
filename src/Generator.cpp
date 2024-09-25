#include "Generator.h"

#include <glm/gtc/integer.hpp>
#include <glm/gtc/random.hpp>
#include <iostream>

#include "renderer/shader/ShaderCompiler.h"

#include "core/Input.h"

namespace Waves
{

Generator::Generator(Vision::RenderDevice* device, FFTCalculator* calc)
  : renderDevice(device), fftCalc(calc), textureSize(calc->GetTextureResolution())
{
  LoadShaders();
  CreateTextures();

  Vision::BufferDesc oceanDesc;
  oceanDesc.DebugName = "Ocean Settings";
  oceanDesc.Type = Vision::BufferType::Uniform;
  oceanDesc.Usage = Vision::BufferUsage::Dynamic;
  oceanDesc.Size = sizeof(OceanSettings);
  oceanDesc.Data = &oceanSettings;
  oceanUBO = renderDevice->CreateBuffer(oceanDesc);
}

Generator::~Generator()
{
  renderDevice->DestroyTexture2D(heightMap);
  renderDevice->DestroyTexture2D(normalMapX);
  renderDevice->DestroyTexture2D(normalMapZ);
  renderDevice->DestroyTexture2D(displacementX);
  renderDevice->DestroyTexture2D(displacementZ);
  renderDevice->DestroyTexture2D(gaussianImage);

  renderDevice->DestroyBuffer(oceanUBO);

  renderDevice->DestroyComputePipeline(computePS);
}

// void Generator::GenerateSpectrum()
// {
//   renderDevice->BeginComputePass();

//   // This is a test FFT using our multipass API
//   renderDevice->SetBufferData(oceanUBO, &oceanSettings,
//   sizeof(OceanSettings)); renderDevice->BindBuffer(oceanUBO);

//   // prepare our spectrum
//   renderDevice->BindImage2D(heightMap, 0);
//   renderDevice->BindImage2D(gaussianImage, 1);
//   renderDevice->DispatchCompute(computePS, "generateSpectrum", { textureSize,
//   textureSize, 1 }); renderDevice->ImageBarrier();

//   PerformFFT(heightMap);

//   renderDevice->EndComputePass();
// }

void Generator::CalculateOcean(float timestep, bool userUpdatedSpectrum)
{
  renderDevice->BeginComputePass();

  // Update our ocean's settings
  oceanSettings.time += timestep;
  renderDevice->SetBufferData(oceanUBO, &oceanSettings, sizeof(OceanSettings));
  renderDevice->BindBuffer(oceanUBO);

  // Update the spectrum if needed
  // if (updateSpectrum || userUpdatedSpectrum)
  {
    updateSpectrum = false;
    GenerateSpectrum();
  }

  // Generate the phillips spectrum based on the given time
  renderDevice->BindImage2D(initialSpectrum, 0);
  renderDevice->BindImage2D(heightMap, 1);
  renderDevice->DispatchCompute(computePS, "propagateWaves", {textureSize, textureSize, 1});

  // Prepare the normal map FFT
  renderDevice->BindImage2D(heightMap, 0);
  renderDevice->BindImage2D(normalMapX, 1);
  renderDevice->BindImage2D(normalMapZ, 2);
  renderDevice->DispatchCompute(computePS, "prepareNormalMap", {textureSize, textureSize, 1});

  // Prepare the displacement map FFT
  renderDevice->BindImage2D(displacementX, 1);
  renderDevice->BindImage2D(displacementZ, 2);
  // renderDevice->BindImage2D(heightMap, 2); // still bound
  renderDevice->DispatchCompute(computePS, "prepareDisplacementMap", {textureSize, textureSize, 1});

  // Ensure that none of our FFTs operate before we are ready
  renderDevice->ImageBarrier();

  fftCalc->EncodeIFFT(heightMap);
  fftCalc->EncodeIFFT(normalMapX);
  fftCalc->EncodeIFFT(normalMapZ);
  fftCalc->EncodeIFFT(displacementX);
  fftCalc->EncodeIFFT(displacementZ);

  // Combine the normal maps after fft
  renderDevice->BindImage2D(normalMapX, 2);
  renderDevice->BindImage2D(normalMapZ, 0);
  renderDevice->DispatchCompute(computePS, "combineNormalMap", {textureSize, textureSize, 1});

  // Also the displacement maps after fft
  renderDevice->BindImage2D(displacementX, 2);
  renderDevice->BindImage2D(displacementZ, 0);
  renderDevice->DispatchCompute(computePS, "combineDisplacementMap", {textureSize, textureSize, 1});

  renderDevice->EndComputePass();
}

void Generator::LoadShaders()
{
  if (computePS)
    renderDevice->DestroyComputePipeline(computePS);

  Vision::ComputePipelineDesc desc;
  Vision::ShaderCompiler compiler;
  compiler.CompileFile("resources/spectrum.compute", desc.ComputeKernels);
  computePS = renderDevice->CreateComputePipeline(desc);
}

void Generator::CreateTextures()
{
  // Delete any textures in case we are regenerating
  if (heightMap)
  {
    renderDevice->DestroyTexture2D(heightMap);
    renderDevice->DestroyTexture2D(normalMapX);
    renderDevice->DestroyTexture2D(normalMapZ);
    renderDevice->DestroyTexture2D(displacementX);
    renderDevice->DestroyTexture2D(displacementZ);
    renderDevice->DestroyTexture2D(gaussianImage);
    renderDevice->DestroyTexture2D(initialSpectrum);
  }

  // Create our blank textures
  Vision::Texture2DDesc desc;
  desc.Width = textureSize;
  desc.Height = textureSize;
  desc.PixelType = Vision::PixelType::RGBA32Float;
  desc.MinFilter = Vision::MinMagFilter::Linear;
  desc.MagFilter = Vision::MinMagFilter::Linear;
  desc.AddressModeS = Vision::EdgeAddressMode::Repeat;
  desc.AddressModeT = Vision::EdgeAddressMode::Repeat;
  desc.WriteOnly = false;
  desc.Data = nullptr;

  heightMap = renderDevice->CreateTexture2D(desc);
  normalMapX = renderDevice->CreateTexture2D(desc);
  normalMapZ = renderDevice->CreateTexture2D(desc);
  displacementX = renderDevice->CreateTexture2D(desc);
  displacementZ = renderDevice->CreateTexture2D(desc);
  gaussianImage = renderDevice->CreateTexture2D(desc);
  initialSpectrum = renderDevice->CreateTexture2D(desc);

  GenerateNoise();
}

void Generator::GenerateNoise()
{
  // Create data for our gaussian image on CPU.
  std::vector<glm::vec4> randomValues(textureSize * textureSize, glm::vec4(0.0f));
  float scale = 1.0 / glm::sqrt(2.0);
  for (int i = 0; i < textureSize * textureSize; i++)
  {
    float x = glm::gaussRand(0.0, 1.0);
    float y = glm::gaussRand(0.0, 1.0);
    glm::vec4 amp = glm::vec4(x, y, 0.0f, 0.0f) * scale;
    randomValues[i] = amp;
  }
  renderDevice->SetTexture2DDataRaw(gaussianImage, randomValues.data());
}

void Generator::GenerateSpectrum()
{
  renderDevice->BindImage2D(gaussianImage, 0, Vision::ImageAccess::ReadOnly);
  renderDevice->BindImage2D(initialSpectrum, 1, Vision::ImageAccess::WriteOnly);
  renderDevice->DispatchCompute(computePS, "generateSpectrum", {textureSize, textureSize, 1});
  renderDevice->ImageBarrier();
}

} // namespace Waves
