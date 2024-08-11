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
}

Generator::~Generator()
{
  renderDevice->DestroyTexture2D(heightMap);
  renderDevice->DestroyTexture2D(normalMap);
  renderDevice->DestroyTexture2D(gaussianImage);

  renderDevice->DestroyComputePipeline(computePS);
}

void Generator::GenerateSpectrum()
{
  renderDevice->BeginComputePass();

  renderDevice->SetComputeTexture(heightMap);
  renderDevice->SetComputeTexture(gaussianImage, 1);
  renderDevice->DispatchCompute(computePS, "generateSpectrum", { textureSize, textureSize, 1 });

  renderDevice->EndComputePass();
}

void Generator::GenerateWaves(float timestep)
{
 renderDevice->BeginComputePass();

 renderDevice->SetComputeTexture(heightMap);
 renderDevice->DispatchCompute(computePS, "horizontalIFFT", { 1, textureSize, 1 });

 // Perform our fft
 renderDevice->ImageBarrier();

 renderDevice->DispatchCompute(computePS, "verticalIFFT", { textureSize, 1, 1 });

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
    renderDevice->DestroyTexture2D(normalMap);
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
  normalMap = renderDevice->CreateTexture2D(desc);
  gaussianImage = renderDevice->CreateTexture2D(desc);

  // create our gaussian texture
  std::vector<glm::vec4> randomValues(textureSize * textureSize, glm::vec4(0.0f));
  float scale = 1.0 / glm::sqrt(2.0);
  for (int i = 0; i < textureSize * textureSize; i++)
  {
    float x = glm::gaussRand(0.0, 1.0);
    float y = glm::gaussRand(0.0, 1.0);
    glm::vec2 amp = glm::vec2(x, y) * scale;
    randomValues[i] = glm::vec4(amp, 0.0, 1.0);
  }
  renderDevice->SetTexture2DDataRaw(gaussianImage, randomValues.data());
}

}
