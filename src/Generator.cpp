#include "Generator.h"

#include <iostream>

namespace Waves
{

Generator::Generator(Vision::RenderDevice *device)
  : renderDevice(device)
{
  // Create our blank textures
  Vision::Texture2DDesc desc;
  desc.Width = 8;
  desc.Height = 8;
  desc.PixelType = Vision::PixelType::RGBA32;
  desc.WriteOnly = false;

  uint32_t data[64];
  for (int i = 0; i < 64; i++)
    data[i] = 0xff0000ff;
  desc.Data = (uint8_t*)&data;

  heightMap = renderDevice->CreateTexture2D(desc);
  normalMap = renderDevice->CreateTexture2D(desc);

  // Create the compute pipeline
  Vision::ComputePipelineDesc pipelineDesc;
  pipelineDesc.FilePath = "resources/imageFFT.glsl";
  computePS = renderDevice->CreateComputePipeline(pipelineDesc);
}

Generator::~Generator()
{
  renderDevice->DestroyTexture2D(heightMap);
  renderDevice->DestroyTexture2D(normalMap);
}

void Generator::GenerateWaves(float timestep)
{
  renderDevice->BeginComputePass();

  renderDevice->SetComputeTexture(heightMap, 0);
  renderDevice->SetComputeTexture(normalMap, 1);
  renderDevice->DispatchCompute(computePS, {1,8,1}); // FFT horizontally

  renderDevice->EndComputePass();
}

}