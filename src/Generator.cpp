#include "Generator.h"

namespace Waves
{

Generator::Generator(Vision::RenderDevice *device)
  : renderDevice(device)
{
  // Create our blank textures
  Vision::Texture2DDesc desc;
  desc.Width = 512;
  desc.Height = 512;
  desc.PixelType = Vision::PixelType::RGBA32;
  desc.WriteOnly = false;
  desc.Data = nullptr;

  heightMap = renderDevice->CreateTexture2D(desc);
  normalMap = renderDevice->CreateTexture2D(desc);

  // Create the compute pipeline
  Vision::ComputePipelineDesc pipelineDesc;
  pipelineDesc.FilePath = "resources/waveCompute.glsl";
  computePS = renderDevice->CreateComputePipeline(pipelineDesc);
}

Generator::~Generator()
{
  renderDevice->DestroyTexture2D(heightMap);
  renderDevice->DestroyTexture2D(normalMap);
}

void Generator::GenerateWaves(float timestep)
{
  renderDevice->SetComputeTexture(heightMap, 0);
  renderDevice->SetComputeTexture(normalMap, 1);
  renderDevice->DispatchCompute(computePS, {512.0f, 512.0f, 1.0f});
}

}