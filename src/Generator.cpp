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
  GenerateTextures();

  Vision::BufferDesc oceanDesc;
  oceanDesc.DebugName = "Ocean Settings";
  oceanDesc.Type = Vision::BufferType::Uniform;
  oceanDesc.Usage = Vision::BufferUsage::Dynamic;
  oceanDesc.Size = sizeof(GeneratorSettings);
  oceanDesc.Data = &oceanSettings;
  oceanUBO = renderDevice->CreateBuffer(oceanDesc);
}

Generator::~Generator()
{
  if (generatedPS)
  {
    renderDevice->DestroyComputePipeline(computePS);
    generatedPS = false;
  }

  renderDevice->DestroyTexture2D(heightMap);
  renderDevice->DestroyTexture2D(displacementMap);
  renderDevice->DestroyTexture2D(gaussianImage);
  renderDevice->DestroyTexture2D(initialSpectrum);

  renderDevice->DestroyBuffer(oceanUBO);
}

void Generator::CalculateOcean(float timestep, bool userUpdatedSpectrum)
{
  renderDevice->BeginComputePass();

  // Update our ocean's settings
  oceanSettings.time += timestep;
  renderDevice->SetBufferData(oceanUBO, &oceanSettings, sizeof(GeneratorSettings));
  renderDevice->BindBuffer(oceanUBO);

  // Update the spectrum if needed
  if (updateSpectrum || userUpdatedSpectrum)
  {
    updateSpectrum = false;
    GenerateSpectrum();
  }

  // Generate the phillips spectrum based on the given time, then prepare the necessary fourier
  // transforms to also calculate the displacement and slopes.
  renderDevice->BindImage2D(initialSpectrum, 0);
  renderDevice->BindImage2D(heightMap, 1);
  renderDevice->BindImage2D(displacementMap, 2);
  renderDevice->DispatchCompute(computePS, "prepareFFT", {textureSize, textureSize, 1});

  // Ensure that none of our FFTs operate before we are ready.
  renderDevice->ImageBarrier();

  fftCalc->EncodeIFFT(heightMap);
  fftCalc->EncodeIFFT(displacementMap);

  renderDevice->ImageBarrier();

  // Once this is done, we compute the jacobian determinant to get the foam texture
  renderDevice->BindBuffer(oceanUBO);
  renderDevice->BindImage2D(displacementMap, 0);
  renderDevice->BindImage2D(jacobian, 3);
  renderDevice->DispatchCompute(computePS, "computeFoam", {textureSize, textureSize, 1});

  renderDevice->EndComputePass();
}

void Generator::LoadShaders(bool reload)
{
  if (!generatedPS || reload)
  {
    if (computePS)
      renderDevice->DestroyComputePipeline(computePS);

    Vision::ShaderCompiler compiler;
    Vision::ComputePipelineDesc desc;
    compiler.CompileFile("resources/spectrum.compute", desc.ComputeKernels, true);
    computePS = renderDevice->CreateComputePipeline(desc);
  }
}

void Generator::GenerateTextures()
{
  // Delete any textures in case we are regenerating
  if (heightMap)
  {
    renderDevice->DestroyTexture2D(heightMap);
    renderDevice->DestroyTexture2D(displacementMap);
    renderDevice->DestroyTexture2D(gaussianImage);
    renderDevice->DestroyTexture2D(initialSpectrum);
    renderDevice->DestroyTexture2D(jacobian);
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
  displacementMap = renderDevice->CreateTexture2D(desc);
  gaussianImage = renderDevice->CreateTexture2D(desc);
  initialSpectrum = renderDevice->CreateTexture2D(desc);

  // The jacobian only has one channel.
  desc.PixelType = Vision::PixelType::R32Float;
  jacobian = renderDevice->CreateTexture2D(desc);

  GenerateNoise();
}

void Generator::GenerateNoise()
{
  // Create data for our gaussian image on CPU.
  std::vector<glm::vec4> randomValues(textureSize * textureSize, glm::vec4(0.0f));

  // Our random pairs of values have std dev of 1 for mag by pythagorean thm.
  float scale = 1.0 / glm::sqrt(2.0);
  for (int i = 0; i < textureSize * textureSize; i++)
    randomValues[i] = glm::gaussRand(glm::vec4(0.0f), glm::vec4(scale));

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
