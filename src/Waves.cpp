#include "Waves.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

#include <imgui.h>

#include "core/Input.h"

namespace Waves
{

WaveApp::WaveApp()
{
  waveRenderer = new WaveRenderer(renderDevice, renderer, GetDisplayWidth(), GetDisplayHeight());
  fftCalculator = new FFTCalculator(renderDevice, textureResolution);

  // Create our three different tiles of ocean of varying sizes.
  for (int i = 0; i < waveRenderer->GetNumRequiredGenerators(); i++)
  {
    // Create our generator and configure
    Generator* generator = new Generator(renderDevice, fftCalculator);
    Generator::OceanSettings& settings = generator->GetOceanSettings();

    // Set the size of the plane based on increasing prime sizes to prevent tiling.
    static float scale = 10.0;
    static float primeFactors[] = {3.0, 7.0, 19.0};
    settings.planeSize = scale * primeFactors[i];

    // Enable bound wavelength to prevent frequencies from adding themselves twice.
    settings.boundWavelength = 1;

    // Each wavelength should be on the smallest plane that it fits on for the most detail.
    settings.wavelengthMax = settings.planeSize / 2.0;
    settings.wavelengthMin = (i == 0) ? 0.0 : scale * primeFactors[i - 1] / 2.0;

    // Add our generator to the array.
    generators.push_back(generator);
  }

  // Create our RenderPass
  Vision::RenderPassDesc rpDesc;
  rpDesc.Framebuffer = 0;
  rpDesc.LoadOp = Vision::LoadOp::Clear;
  rpDesc.StoreOp = Vision::StoreOp::Store;
  rpDesc.ClearColor = {0.5f, 0.5f, 0.5f, 1.0f};
  renderPass = renderDevice->CreateRenderPass(rpDesc);
}

WaveApp::~WaveApp()
{
  renderDevice->DestroyRenderPass(renderPass);

  delete fftCalculator;
  delete waveRenderer;
  for (auto* generator : generators)
    delete generator;
}

void WaveApp::OnUpdate(float timestep)
{
  waveRenderer->UpdateCamera(timestep);

  // Press R to reload shaders
  if (Vision::Input::KeyPress(SDL_SCANCODE_R))
  {
    for (auto* generator : generators)
      generator->LoadShaders();
    waveRenderer->LoadShaders();
  }

  // If our application is occluded, we skip the rendering to avoid flooding memory with commands
  // that won't be executed.
  if (!ShouldRender())
    return;

  // Begin recording commands
  renderDevice->BeginCommandBuffer();

  // First, we do the waves pass
  for (auto* generator : generators)
    generator->CalculateOcean(timestep, updateSpectrum);

  // If we have updated our ocean spectrum, we don't need to again until it's changed.
  updateSpectrum = false;

  // Then we do our the render pass
  renderDevice->BeginRenderPass(renderPass);
  waveRenderer->Render(generators);
  DrawUI();
  renderDevice->EndRenderPass();

  // And present to the the screen
  renderDevice->SchedulePresentation();
  renderDevice->SubmitCommandBuffer();
}

void WaveApp::DrawUI()
{
  uiRenderer->Begin();
  if (ImGui::Begin("Settings"))
  {
    // Simulation Settings
    if (ImGui::CollapsingHeader("Simulation"))
    {
      Generator::OceanSettings& settings = generators[0]->GetOceanSettings();

      updateSpectrum |= ImGui::DragFloat2("Wind Velocity", &settings.windVelocity[0], 0.25f);
      updateSpectrum |= ImGui::DragFloat("Gravity", &settings.gravity, 0.05f);
      updateSpectrum |= ImGui::DragFloat("Scale", &settings.scale, 0.05f);

      if (updateSpectrum)
      {
        for (auto& generator : generators)
        {
          Generator::OceanSettings& toChange = generator->GetOceanSettings();
          toChange.windVelocity = settings.windVelocity;
          toChange.gravity = settings.gravity;
          toChange.scale = settings.scale;
        }
      }
    }

    // Rendering Settings
    if (ImGui::CollapsingHeader("Rendering"))
    {
      WaveRenderData& data = waveRenderer->GetWaveRenderData();

      ImGui::ColorEdit4("Wave Color", &data.waveColor[0]);
      ImGui::ColorEdit4("Sky Color", &data.skyColor[0]);
      ImGui::ColorEdit4("Sun Color", &data.sunColor[0]);
      ImGui::DragFloat("Sun Size", &data.sunViewAngle, 0.1f, 0.0f, 40.0f, "%.1f");
      ImGui::DragFloat("Sun Fade", &data.sunFalloffAngle, 0.1f, 0.0f, 40.0f, "%.1f");
    }
  }
  ImGui::End();

  uiRenderer->End();
}

void WaveApp::OnResize(float width, float height)
{
  waveRenderer->Resize(width, height);
}

} // namespace Waves
