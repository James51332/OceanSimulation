#include "Generator.h"

#include <glm/gtc/random.hpp>
#include <iostream>

#include "renderer/shader/ShaderCompiler.h"

#include "core/Input.h"

namespace Waves
{

Generator::Generator(Vision::RenderDevice *device)
  : renderDevice(device)
{
  LoadShaders();
  CreateTextures();

  Vision::BufferDesc desc;
  desc.DebugName = "Ocean Settings";
  desc.Type = Vision::BufferType::Uniform;
  desc.Usage = Vision::BufferUsage::Dynamic;
  desc.Size = sizeof(OceanSettings);
  desc.Data = &oceanSettings;
  uniformBuffer = renderDevice->CreateBuffer(desc);
}

Generator::~Generator()
{
  renderDevice->DestroyTexture2D(heightMap);
  renderDevice->DestroyTexture2D(normalMapX);
  renderDevice->DestroyTexture2D(normalMapZ);
  renderDevice->DestroyTexture2D(gaussianImage);

  renderDevice->DestroyBuffer(uniformBuffer);

  renderDevice->DestroyComputePipeline(computePS);
}

void Generator::CalculateOcean(float timestep)
{
  renderDevice->BeginComputePass();

  // Update our ocean's settings
  oceanSettings.time += timestep;
  renderDevice->SetBufferData(uniformBuffer, &oceanSettings, sizeof(OceanSettings));
  renderDevice->SetComputeBuffer(uniformBuffer);

  // Generate the phillips spectrum based on the given time
  renderDevice->SetComputeTexture(heightMap, 0);
  renderDevice->SetComputeTexture(gaussianImage, 1);
  renderDevice->DispatchCompute(computePS, "generateSpectrum", { textureSize, textureSize, 1 });

  // Prepare the normal map FFT
  renderDevice->SetComputeTexture(normalMapX, 0);
  renderDevice->SetComputeTexture(normalMapZ, 1);
  renderDevice->SetComputeTexture(heightMap, 2);
  renderDevice->DispatchCompute(computePS, "prepareNormalMap", { textureSize, textureSize, 1 });

  // Prepare the displacement map FFT
  renderDevice->SetComputeTexture(displacementX, 0);
  renderDevice->SetComputeTexture(displacementZ, 1);
  // renderDevice->SetComputeTexture(heightMap, 2); // still bound
  renderDevice->DispatchCompute(computePS, "prepareDisplacementMap", { textureSize, textureSize, 1 });

  // Ensure that none of our FFTs operate before we are ready
  renderDevice->ImageBarrier();

  // Perform all of our necessary FFTs
  auto fft = [&](Vision::ID image) {
    renderDevice->SetComputeTexture(image);
    renderDevice->DispatchCompute(computePS, "horizontalIFFT", { 1, textureSize, 1 });
    renderDevice->ImageBarrier();
    renderDevice->DispatchCompute(computePS, "verticalIFFT", { textureSize, 1, 1 });
  };

  fft(heightMap);
  fft(normalMapX);
  fft(normalMapZ);
  fft(displacementX);
  fft(displacementZ);

  // Wait until our FFTs are done before finalizing
  renderDevice->ImageBarrier();

  // Combine the normal maps after fft
  renderDevice->SetComputeTexture(normalMapX, 0);
  renderDevice->SetComputeTexture(normalMapZ, 1);
  renderDevice->DispatchCompute(computePS, "combineNormalMap", { textureSize, textureSize, 1 });

  // Also the displacement maps after fft
  renderDevice->SetComputeTexture(displacementX, 0);
  renderDevice->SetComputeTexture(displacementZ, 1);
  renderDevice->DispatchCompute(computePS, "combineDisplacementMap", { textureSize, textureSize, 1 });

  renderDevice->EndComputePass();
}

void Generator::LoadShaders()
{
  if (computePS)
    renderDevice->DestroyComputePipeline(computePS);

  Vision::ComputePipelineDesc desc;
  Vision::ShaderCompiler compiler;
  compiler.CompileFile("resources/fft.compute", desc.ComputeKernels);
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
  }

  // Create our blank textures
  Vision::Texture2DDesc desc;
  desc.Width = textureSize;
  desc.Height = textureSize;
  desc.PixelType = Vision::PixelType::RGBA32Float;
  desc.MinFilter = Vision::MinMagFilter::Linear;
  desc.MagFilter = Vision::MinMagFilter::Linear;
  desc.WriteOnly = false;
  desc.Data = nullptr;

  heightMap = renderDevice->CreateTexture2D(desc);
  normalMapX = renderDevice->CreateTexture2D(desc);
  normalMapZ = renderDevice->CreateTexture2D(desc);
  displacementX = renderDevice->CreateTexture2D(desc);
  displacementZ = renderDevice->CreateTexture2D(desc);
  gaussianImage = renderDevice->CreateTexture2D(desc);

  // Create data for our sgaussian image on CPU
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

}
