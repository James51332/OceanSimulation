#include "Generator.h"

#include <glm/gtc/random.hpp>
#include <glm/gtc/integer.hpp>
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

  Vision::BufferDesc oceanDesc;
  oceanDesc.DebugName = "Ocean Settings";
  oceanDesc.Type = Vision::BufferType::Uniform;
  oceanDesc.Usage = Vision::BufferUsage::Dynamic;
  oceanDesc.Size = sizeof(OceanSettings);
  oceanDesc.Data = &oceanSettings;
  oceanUBO = renderDevice->CreateBuffer(oceanDesc);

  // Create an array to populate our FFT UBO.
  int numPasses = glm::log2(textureSize) * 2; // horizontal and vertical
  std::vector<FFTPass> passes;
  for (int i = 0; i < numPasses; i++)
  {
    FFTPass pass;
    pass.passNumber = i % (numPasses / 2);
    pass.vertical = i >= (numPasses / 2) ? 1 : 0;
    pass.totalSize = textureSize;
    passes.push_back(pass);
  }

  Vision::BufferDesc fftDesc;
  fftDesc.DebugName = "FFTPass UBO";
  fftDesc.Type = Vision::BufferType::Uniform;
  fftDesc.Usage = Vision::BufferUsage::Static;
  fftDesc.Size = sizeof(FFTPass) * numPasses;
  fftDesc.Data = passes.data();
  fftUBO = renderDevice->CreateBuffer(fftDesc);
}

Generator::~Generator()
{
  renderDevice->DestroyTexture2D(heightMap);
  renderDevice->DestroyTexture2D(normalMapX);
  renderDevice->DestroyTexture2D(normalMapZ);
  renderDevice->DestroyTexture2D(displacementX);
  renderDevice->DestroyTexture2D(displacementZ);
  renderDevice->DestroyTexture2D(gaussianImage);
  renderDevice->DestroyTexture2D(tempImage);

  renderDevice->DestroyBuffer(oceanUBO);
  renderDevice->DestroyBuffer(fftUBO);

  renderDevice->DestroyComputePipeline(computePS);
}

void Generator::GenerateSpectrum()
{
  renderDevice->BeginComputePass();
  
  // This is a test FFT using our multipass API
  renderDevice->SetBufferData(oceanUBO, &oceanSettings, sizeof(OceanSettings));
  renderDevice->BindBuffer(oceanUBO);

  // prepare our spectrum
  renderDevice->BindImage2D(heightMap, 0);
  renderDevice->BindImage2D(gaussianImage, 1);
  renderDevice->DispatchCompute(computePS, "generateSpectrum", { textureSize, textureSize, 1 });
  renderDevice->ImageBarrier();

  PerformFFT(heightMap);

  renderDevice->EndComputePass();
}

void Generator::CalculateOcean(float timestep)
{
  renderDevice->BeginComputePass();

  // Update our ocean's settings
  oceanSettings.time += timestep;
  renderDevice->SetBufferData(oceanUBO, &oceanSettings, sizeof(OceanSettings));
  renderDevice->BindBuffer(oceanUBO);

  // Generate the phillips spectrum based on the given time
  renderDevice->BindImage2D(heightMap, 0);
  renderDevice->BindImage2D(gaussianImage, 1);
  renderDevice->DispatchCompute(computePS, "generateSpectrum", {textureSize, textureSize, 1});

  // Prepare the normal map FFT
  renderDevice->BindImage2D(normalMapX, 0);
  renderDevice->BindImage2D(normalMapZ, 1);
  renderDevice->BindImage2D(heightMap, 2);
  renderDevice->DispatchCompute(computePS, "prepareNormalMap", { textureSize, textureSize, 1 });

  // Prepare the displacement map FFT
  renderDevice->BindImage2D(displacementX, 0);
  renderDevice->BindImage2D(displacementZ, 1);
  // renderDevice->BindImage2D(heightMap, 2); // still bound
  renderDevice->DispatchCompute(computePS, "prepareDisplacementMap", { textureSize, textureSize, 1 });

  // Ensure that none of our FFTs operate before we are ready
  renderDevice->ImageBarrier();

  PerformFFT(heightMap);
  PerformFFT(normalMapX);
  PerformFFT(normalMapZ);
  PerformFFT(displacementX);
  PerformFFT(displacementZ);

  // Wait until our FFTs are done before finalizing
  renderDevice->ImageBarrier();

  // Combine the normal maps after fft
  renderDevice->BindImage2D(normalMapX, 0);
  renderDevice->BindImage2D(normalMapZ, 1);
  renderDevice->DispatchCompute(computePS, "combineNormalMap", { textureSize, textureSize, 1 });

  // Also the displacement maps after fft
  renderDevice->BindImage2D(displacementX, 0);
  renderDevice->BindImage2D(displacementZ, 1);
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
    renderDevice->DestroyTexture2D(tempImage);
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
  tempImage = renderDevice->CreateTexture2D(desc);

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

void Generator::PerformFFT(Vision::ID srcImage)
{
  // Function to bind appropriate image
  bool workImgAsInput = false;
  auto bindImages = [&]() 
  {
    if (!workImgAsInput)
    {
      renderDevice->BindImage2D(srcImage, 0, Vision::ImageAccess::ReadOnly);
      renderDevice->BindImage2D(tempImage, 1, Vision::ImageAccess::WriteOnly);
    }
    else
    {
      renderDevice->BindImage2D(srcImage, 1, Vision::ImageAccess::WriteOnly);
      renderDevice->BindImage2D(tempImage, 0, Vision::ImageAccess::ReadOnly);
    }

    workImgAsInput = !workImgAsInput;
  };

  // Swap low frequencies to edges
  bindImages();
  renderDevice->DispatchCompute(computePS, "fftShift", { textureSize, textureSize, 1 });
  renderDevice->ImageBarrier();

  // Bit-reversal to prepare for Cooley-Tukey FFT
  bindImages();
  renderDevice->DispatchCompute(computePS, "imageReversal", { textureSize, textureSize, 1 });
  renderDevice->ImageBarrier();

  // Perform all of our passes
  int numPasses = glm::log2(textureSize) * 2;
  for (int i = 0; i < numPasses; i++)
  {
    renderDevice->BindBuffer(fftUBO, 0, i * sizeof(FFTPass), sizeof(FFTPass));

    bindImages();
    renderDevice->DispatchCompute(computePS, "fft", { textureSize, 1, 1 });
    renderDevice->ImageBarrier();
  }
}

}
