#include "Generator.h"

#include <iostream>

#include "renderer/shader/ShaderCompiler.h"

#include "core/Input.h"

namespace Waves
{

Generator::Generator(Vision::RenderDevice *device)
  : renderDevice(device)
{
  // Create our blank textures
  Vision::Texture2DDesc desc;
  desc.Width = textureSize;
  desc.Height = textureSize;
  desc.PixelType = Vision::PixelType::RGBA32Float;
  desc.WriteOnly = false;
  desc.Data = nullptr;

  heightMap = renderDevice->CreateTexture2D(desc);
  normalMap = renderDevice->CreateTexture2D(desc);

  // Create the compute pipeline
  Vision::ComputePipelineDesc pipelineDesc;
  pipelineDesc.ComputeKernels = Vision::ShaderCompiler().CompileFile("resources/imageFFT.glsl");
  computePS = renderDevice->CreateComputePipeline(pipelineDesc);
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
  if (Vision::Input::KeyDown(SDL_SCANCODE_Z))
  {
    renderDevice->DestroyComputePipeline(computePS);
    Vision::ComputePipelineDesc pipelineDesc;
    pipelineDesc.ComputeKernels = Vision::ShaderCompiler().CompileFile("resources/imageFFT.glsl");
    computePS = renderDevice->CreateComputePipeline(pipelineDesc);
  }

  renderDevice->BeginComputePass();

  renderDevice->SetComputeTexture(heightMap, 0);
  renderDevice->SetComputeTexture(normalMap, 1);

  // Perform our fft
  renderDevice->DispatchCompute(computePS, "horizontalFFT", { 1, textureSize, 1 });
  renderDevice->ImageBarrier();
  renderDevice->DispatchCompute(computePS, "verticalFFT", { textureSize, 1, 1 });

  renderDevice->EndComputePass();
}

}
