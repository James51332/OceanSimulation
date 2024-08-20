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

  Vision::BufferDesc fftDesc;
  fftDesc.Type = Vision::BufferType::Uniform;
  fftDesc.Usage = Vision::BufferUsage::Dynamic;
  fftDesc.Size = sizeof(glm::vec4);
  fftDesc.Data = nullptr;
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
  renderDevice->SetComputeBuffer(oceanUBO);

  // prepare our spectrum
  renderDevice->SetComputeImage(heightMap, 0);
  renderDevice->SetComputeImage(gaussianImage, 1);
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
  renderDevice->SetComputeBuffer(oceanUBO);

  // Generate the phillips spectrum based on the given time
  renderDevice->SetComputeImage(heightMap, 0);
  renderDevice->SetComputeImage(gaussianImage, 1);
  renderDevice->DispatchCompute(computePS, "generateSpectrum", {textureSize, textureSize, 1});

  // Prepare the normal map FFT
  renderDevice->SetComputeImage(normalMapX, 0);
  renderDevice->SetComputeImage(normalMapZ, 1);
  renderDevice->SetComputeImage(heightMap, 2);
  renderDevice->DispatchCompute(computePS, "prepareNormalMap", { textureSize, textureSize, 1 });

  // Prepare the displacement map FFT
  renderDevice->SetComputeImage(displacementX, 0);
  renderDevice->SetComputeImage(displacementZ, 1);
  // renderDevice->SetComputeImage(heightMap, 2); // still bound
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
  renderDevice->SetComputeImage(normalMapX, 0);
  renderDevice->SetComputeImage(normalMapZ, 1);
  renderDevice->DispatchCompute(computePS, "combineNormalMap", { textureSize, textureSize, 1 });

  // Also the displacement maps after fft
  renderDevice->SetComputeImage(displacementX, 0);
  renderDevice->SetComputeImage(displacementZ, 1);
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
  // Switch to FFT settings UBO
  renderDevice->SetComputeBuffer(fftUBO);

  // Function to bind appropriate image
  bool workImgAsInput = false;
  auto bindImages = [&]() 
  {
    if (!workImgAsInput)
    {
      renderDevice->SetComputeImage(srcImage, 0, Vision::ComputeImageAccess::ReadOnly);
      renderDevice->SetComputeImage(tempImage, 1, Vision::ComputeImageAccess::WriteOnly);
    }
    else
    {
      renderDevice->SetComputeImage(srcImage, 1, Vision::ComputeImageAccess::WriteOnly);
      renderDevice->SetComputeImage(tempImage, 0, Vision::ComputeImageAccess::ReadOnly);
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

  // Horizontal FFT
  int logSize = glm::log2(textureSize);
  for (int i = 0; i < logSize; i++)
  {
    glm::ivec4 settings = glm::ivec4(i, false, 0, 0);
    renderDevice->SetBufferData(fftUBO, &settings, sizeof(glm::ivec4));
    renderDevice->SetComputeBuffer(fftUBO);

    bindImages();
    renderDevice->DispatchCompute(computePS, "fft", { textureSize, 1, 1 });
    renderDevice->ImageBarrier();
  }

  // Vertical FFT
  for (int i = 0; i < logSize; i++)
  {
    glm::ivec4 settings = glm::ivec4(i, true, 0, 0);
    renderDevice->SetBufferData(fftUBO, &settings, sizeof(glm::ivec4));
    renderDevice->SetComputeBuffer(fftUBO);

    bindImages();
    renderDevice->DispatchCompute(computePS, "fft", { textureSize, 1, 1 });
    renderDevice->ImageBarrier();
  }
}

}
