#include "FFTCalculator.h"

#include <glm/gtc/integer.hpp>

#include "renderer/shader/ShaderCompiler.h"

namespace Waves
{

FFTCalculator::FFTCalculator(Vision::RenderDevice* renderDevice, std::size_t size)
  : device(renderDevice), textureSize(size)
{
  // Create an array to populate our FFT UBO.
  numPasses = glm::log2(textureSize) * 2; // horizontal and vertical
  std::vector<FFTPass> passes;
  for (int i = 0; i < numPasses; i++)
  {
    FFTPass pass;
    pass.passNumber = i % (numPasses / 2);
    pass.vertical = i >= (numPasses / 2) ? 1 : 0;
    pass.totalSize = textureSize;
    passes.push_back(pass);
  }

  // Allocate and populate our GPU memory with the pass information.
  Vision::BufferDesc fftDesc;
  fftDesc.DebugName = "FFT Calculator UBO";
  fftDesc.Type = Vision::BufferType::Uniform;
  fftDesc.Usage = Vision::BufferUsage::Static;
  fftDesc.Size = sizeof(FFTPass) * numPasses;
  fftDesc.Data = passes.data();
  fftUBO = device->CreateBuffer(fftDesc);

  // Create our work image
  Vision::Texture2DDesc imgDesc;
  imgDesc.Width = textureSize;
  imgDesc.Height = textureSize;
  imgDesc.PixelType = Vision::PixelType::RGBA32Float;
  imgDesc.MinFilter = Vision::MinMagFilter::Linear;
  imgDesc.MagFilter = Vision::MinMagFilter::Linear;
  imgDesc.AddressModeS = Vision::EdgeAddressMode::Repeat;
  imgDesc.AddressModeT = Vision::EdgeAddressMode::Repeat;
  imgDesc.WriteOnly = false;
  imgDesc.Data = nullptr;
  workImage = device->CreateTexture2D(imgDesc);

  // Don't recompile these shaders if we've done it once.
  if (!generatedPS)
  {
    // Load and compile our FFT compute shaders to create the compute pipeline.
    Vision::ShaderCompiler shaderCompiler;
    Vision::ComputePipelineDesc pipelineDesc;
    pipelineDesc.ComputeKernels = shaderCompiler.CompileFile("resources/fft.compute", true);
    fftPS = device->CreateComputePipeline(pipelineDesc);

    generatedPS = true;
  }
}

FFTCalculator::~FFTCalculator()
{
  device->DestroyBuffer(fftUBO);
  device->DestroyTexture2D(workImage);
  device->DestroyComputePipeline(fftPS);
}

void FFTCalculator::EncodeIFFT(Vision::ID image)
{
  // Lamdba to bind appropriate image as we ping-pong.
  bool workImgAsInput = false;
  auto bindImages = [&]()
  {
    if (!workImgAsInput)
    {
      device->BindImage2D(image, 0, Vision::ImageAccess::ReadOnly);
      device->BindImage2D(workImage, 1, Vision::ImageAccess::WriteOnly);
    }
    else
    {
      device->BindImage2D(image, 1, Vision::ImageAccess::WriteOnly);
      device->BindImage2D(workImage, 0, Vision::ImageAccess::ReadOnly);
    }

    workImgAsInput = !workImgAsInput;
  };

  // Swap low frequencies to edges.
  bindImages();
  device->DispatchCompute(fftPS, "fftShift", {textureSize, textureSize, 1});

  // We must make sure our modifications to the image are coherent and visible after each command.
  device->ImageBarrier();

  // Perform our index bit-reversal to prepare for cooley-tukey FFT.
  bindImages();
  device->DispatchCompute(fftPS, "imageReversal", {textureSize, textureSize, 1});
  device->ImageBarrier();

  // Encode our iterative passes.
  for (int i = 0; i < numPasses; i++)
  {
    device->BindBuffer(fftUBO, 0, i * sizeof(FFTPass), sizeof(FFTPass));

    bindImages();
    device->DispatchCompute(fftPS, "fft", {textureSize, 1, 1});
    device->ImageBarrier();
  }
}

} // namespace Waves