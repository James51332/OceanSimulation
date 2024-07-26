#include "Generator.h"

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

  std::vector<glm::vec2> data = {
    { 2.125000f, 0.500000f },
    { -0.426777f, 0.478553f },
    { 0.125000f, 0.000000f },
    { 0.030330f, -0.021447f },
    { 0.125000f, 0.000000f },
    { -0.073223f, -0.228553f },
    { 0.125000f, 0.000000f },
    { -1.030330f, -0.728553f }
  };

  Vision::BufferDesc desc;
  desc.Type = Vision::BufferType::ShaderStorage;
  desc.Size = sizeof(glm::vec4) * data.size();
  desc.Data = data.data();
  desc.DebugName = "FFT SSBO";
  desc.Usage = Vision::BufferUsage::Dynamic;
  ssbo = device->CreateBuffer(desc);

  device->BeginCommandBuffer();
  device->BeginComputePass();

  device->SetComputeBuffer(ssbo);
  device->DispatchCompute(computePS, "fft", {1,1,1});

  device->EndComputePass();
  device->SubmitCommandBuffer(true);

  glm::vec2* ssboData;
  device->MapBufferData(ssbo, (void**)&ssboData, sizeof(glm::vec2) * data.size());
  if (ssboData)
  {
    for (int i = 0; i < data.size(); i++)
    {
      std::cout << "{ " << ssboData[i].x << ", " << ssboData[i].y << " }" << std::endl;
    }
  }
  device->FreeBufferData(ssbo, (void**)&ssboData);
}

Generator::~Generator()
{
  renderDevice->DestroyTexture2D(heightMap);
  renderDevice->DestroyTexture2D(normalMap);

  renderDevice->DestroyComputePipeline(computePS);
}

void Generator::GenerateSpectrum()
{
  renderDevice->BeginComputePass();

  renderDevice->SetComputeTexture(heightMap);
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
  if (heightMap)
  {
    renderDevice->DestroyTexture2D(heightMap);
    renderDevice->DestroyTexture2D(normalMap);
  }

  // Create our blank textures
  Vision::Texture2DDesc desc;
  desc.Width = textureSize;
  desc.Height = textureSize;
  desc.PixelType = Vision::PixelType::RGBA32Float;
  desc.WriteOnly = false;
  desc.Data = nullptr;

  heightMap = renderDevice->CreateTexture2D(desc);
  normalMap = renderDevice->CreateTexture2D(desc);
}

}
