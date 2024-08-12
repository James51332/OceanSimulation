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
  CreateTextures();
  LoadShaders();

  Vision::BufferDesc desc;
  desc.DebugName = "FFT Settings";
  desc.Type = Vision::BufferType::Uniform;
  desc.Usage = Vision::BufferUsage::Dynamic;
  desc.Size = sizeof(glm::vec4);
  desc.Data = nullptr;
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

void FFT(Vision::ID image, Vision::ID computePS, std::size_t size, Vision::RenderDevice *device)
{
  device->SetComputeTexture(image);
  device->DispatchCompute(computePS, "horizontalIFFT", {1, size, 1});
  device->ImageBarrier();
  device->DispatchCompute(computePS, "verticalIFFT", {size, 1, 1});
}

void Generator::CalculateOcean(float timestep)
{
  renderDevice->BeginComputePass();

  // Update our uniform data
  time += timestep;
  glm::vec4 data(time, 40.0f, 0.0f, 0.0f);
  renderDevice->SetBufferData(uniformBuffer, &data, sizeof(glm::vec4));
  renderDevice->SetComputeBuffer(uniformBuffer);

  renderDevice->SetComputeTexture(heightMap);

  // Generate the phillips spectrum based on the given time
  renderDevice->SetComputeTexture(gaussianImage, 1);
  renderDevice->DispatchCompute(computePS, "generateSpectrum", { textureSize, textureSize, 1 });

  // Generate the normal map fft
  renderDevice->SetComputeTexture(normalMapX, 1);
  renderDevice->SetComputeTexture(normalMapZ, 2);
  renderDevice->DispatchCompute(computePS, "prepareNormalMap", {textureSize, textureSize, 1});

  // Generate the normal map fft
  renderDevice->SetComputeTexture(displacementX, 1);
  renderDevice->SetComputeTexture(displacementZ, 2);
  renderDevice->DispatchCompute(computePS, "prepareDisplacementMap", {textureSize, textureSize, 1});

  FFT(heightMap, computePS, textureSize, renderDevice);
  FFT(normalMapX, computePS, textureSize, renderDevice);
  FFT(normalMapZ, computePS, textureSize, renderDevice);
  FFT(displacementX, computePS, textureSize, renderDevice);
  FFT(displacementZ, computePS, textureSize, renderDevice);

  // Combine the normal maps after fft
  renderDevice->SetComputeTexture(normalMapX, 1);
  renderDevice->SetComputeTexture(normalMapZ, 2);
  renderDevice->DispatchCompute(computePS, "combineNormalMap", {textureSize, textureSize, 1});

  // Also the displacement maps after fft
  renderDevice->SetComputeTexture(displacementX, 1);
  renderDevice->SetComputeTexture(displacementZ, 2);
  renderDevice->DispatchCompute(computePS, "combineDisplacementMap", {textureSize, textureSize, 1});
  renderDevice->EndComputePass();
}

void Generator::LoadShaders()
{
  if (computePS)
    renderDevice->DestroyComputePipeline(computePS);

  Vision::ComputePipelineDesc desc;
  Vision::ShaderCompiler compiler;
  
  std::vector<Vision::ShaderSPIRV> imageFuncs = compiler.CompileFile("resources/imageFFT.glsl");
  std::vector<Vision::ShaderSPIRV> bufferFuncs = compiler.CompileFile("resources/bufferFFT.glsl");
  desc.ComputeKernels = imageFuncs;
  desc.ComputeKernels.insert(desc.ComputeKernels.end(), bufferFuncs.begin(), bufferFuncs.end()); // concatenate via a copy
  computePS = renderDevice->CreateComputePipeline(desc);
}

void Generator::CreateTextures()
{
  // delete any textures we may have remaining
  if (heightMap)
  {
    renderDevice->DestroyTexture2D(heightMap);
    renderDevice->DestroyTexture2D(normalMapX);
    renderDevice->DestroyTexture2D(normalMapZ);
    renderDevice->DestroyTexture2D(displacementX);
    renderDevice->DestroyTexture2D(displacementZ);
    renderDevice->DestroyTexture2D(gaussianImage);
  }

  // create our blank textures
  Vision::Texture2DDesc desc;
  desc.Width = textureSize;
  desc.Height = textureSize;
  desc.PixelType = Vision::PixelType::RGBA32Float;
  desc.WriteOnly = false;
  desc.Data = nullptr;

  heightMap = renderDevice->CreateTexture2D(desc);
  normalMapX = renderDevice->CreateTexture2D(desc);
  normalMapZ = renderDevice->CreateTexture2D(desc);
  displacementX = renderDevice->CreateTexture2D(desc);
  displacementZ = renderDevice->CreateTexture2D(desc);
  gaussianImage = renderDevice->CreateTexture2D(desc);

  // create our gaussian texture
  std::vector<glm::vec2> randomValues(textureSize * textureSize, glm::vec4(0.0f));
  float scale = 1.0 / glm::sqrt(2.0);
  for (int i = 0; i < textureSize * textureSize; i++)
  {
    float x = glm::gaussRand(0.0, 1.0);
    float y = glm::gaussRand(0.0, 1.0);
    glm::vec2 amp = glm::vec2(x, y) * scale;
    randomValues[i] = amp;
  }
  renderDevice->SetTexture2DDataRaw(gaussianImage, randomValues.data());
}

}
