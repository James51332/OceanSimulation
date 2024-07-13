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
  desc.Data = nullptr;

  heightMap = renderDevice->CreateTexture2D(desc);
  normalMap = renderDevice->CreateTexture2D(desc);

  // Create the compute pipeline
  Vision::ComputePipelineDesc pipelineDesc;
  pipelineDesc.FilePath = "resources/waveCompute.glsl";
  computePS = renderDevice->CreateComputePipeline(pipelineDesc);

  Vision::BufferDesc ssboDesc;
  ssboDesc.Type = Vision::BufferType::ShaderStorage;
  ssboDesc.Size = sizeof(glm::vec2) * 8;
  ssboDesc.Usage = Vision::BufferUsage::Dynamic;

  std::vector<glm::vec2> data(8, {0.0f, 0.0f});
  data[0] = {4.0f, 0.0f};
  data[1] = {0.0f, 0.0f};
  data[2] = {0.0f, 0.0f};
  data[3] = {0.0f, 0.0f};
  data[4] = {4.0f, 0.0f};
  data[5] = {0.0f, 0.0f};
  data[6] = {0.0f, 0.0f};
  data[7] = {0.0f, 0.0f};

  ssboDesc.DebugName = "FFT SSBO";
  ssboDesc.Data = data.data();
  buffer = renderDevice->CreateBuffer(ssboDesc);
}

Generator::~Generator()
{
  renderDevice->DestroyTexture2D(heightMap);
  renderDevice->DestroyTexture2D(normalMap);
}

void Generator::GenerateWaves(float timestep)
{
  // renderDevice->SetComputeTexture(heightMap, 0);
  // renderDevice->SetComputeTexture(normalMap, 1);
  renderDevice->BeginCommandBuffer();
  renderDevice->BeginComputePass();
  renderDevice->SetComputeBuffer(buffer, 0);
  renderDevice->DispatchCompute(computePS, {1,1,1}); // FFT horizontally

  renderDevice->EndComputePass();
  renderDevice->SubmitCommandBuffer(true);

  glm::vec2 *buf;
  renderDevice->MapBufferData(buffer, (void **)&buf, sizeof(glm::vec2) * 8);
  for (int i = 0; i < 8; i++)
  {
    std::cout << "(" << buf[i].x << ", " << buf[i].y << ") ";
  }
  std::cout << std::endl;
  renderDevice->FreeBufferData(buffer, (void **)&buf);

  // renderDevice->BeginCommandBuffer();
}

}