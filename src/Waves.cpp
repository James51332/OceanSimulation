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
    static float primeFactors[] = {13.0, 71.0, 131.0};
    settings.planeSize = primeFactors[i];

    // Enable bound wavelength to prevent frequencies from adding themselves twice.
    settings.boundWavelength = 0;

    // Each wavelength should be on the smallest plane that it fits on for the most detail.
    settings.wavelengthMax = settings.planeSize / 2.0;
    settings.wavelengthMin = (i == 0) ? 0.0 : primeFactors[i - 1] / 2.0;

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
  // Calculate the frame time even when the window isn't open.
  static double lastTicks = SDL_GetTicks();
  double curTicks = SDL_GetTicks();
  double frameTime = curTicks - lastTicks;

  // Accumulate the frame time using exponential decay
  static double weightedFrameTime = 1.0f / 60.0f;
  weightedFrameTime = frameTime * 0.1f + weightedFrameTime * 0.9f;

  // Update the last frame time
  lastTicks = curTicks;

  uiRenderer->Begin();
  if (ImGui::Begin("Control Panel"))
  {
    // Performance Metrics
    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
    {

      // Print the FPS and frame time
      ImGui::Text("FPS: %.1f", (1000.0f / weightedFrameTime));
      ImGui::Text("Frame Time: %.1fms", weightedFrameTime);
    }

    // Simulation Settings
    if (ImGui::CollapsingHeader("Simulation"))
    {
      Generator::OceanSettings& settings = generators[0]->GetOceanSettings();
      Generator::OceanSettings& settings1 = generators[1]->GetOceanSettings();
      Generator::OceanSettings& settings2 = generators[2]->GetOceanSettings();

      // clang-format off
      updateSpectrum |= ImGui::DragFloat2("Wind Velocity", &settings.windVelocity[0], 0.25f, 0.0f, 20.0f, "%.2f");
      updateSpectrum |= ImGui::DragFloat("Gravity", &settings.gravity, 0.05f, 1.0f, 20.0f, "%.2f");
      updateSpectrum |= ImGui::DragFloat("Scale", &settings.scale, 0.05f, 0.05f, 5.0f, "%.2f");

      static const char* text[] = {"Plane 1", "Plane 2", "Plane 3"};
      updateSpectrum |= ImGui::DragFloat(text[0], &settings.planeSize, 0.1f, 1.0f, settings1.planeSize, "%.1f");
      updateSpectrum |= ImGui::DragFloat(text[1], &settings1.planeSize, 1.f, settings.planeSize, settings2.planeSize, "%.0f");
      updateSpectrum |= ImGui::DragFloat(text[2], &settings2.planeSize, 5.0f, settings1.planeSize, 500.0f, "%.0f");
      // clang-format on

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
      ImGui::ColorEdit4("Scatter Color", &data.scatterColor[0]);
      ImGui::ColorEdit4("Sky Color", &data.skyColor[0]);
      ImGui::ColorEdit4("Sun Color", &data.sunColor[0]);
      ImGui::DragFloat3("Sun Direction", &data.lightDirection[0], 0.01f, -1.0f, 1.0f, "%.2f");
      ImGui::DragFloat("Sun Size", &data.sunViewAngle, 0.1f, 0.0f, 40.0f, "%.1f");
      ImGui::DragFloat("Sun Fade", &data.sunFalloffAngle, 0.1f, 0.0f, 40.0f, "%.1f");
      ImGui::DragFloat("Displacement", &data.displacementScale, 0.01f, 0.0f, 2.0f, "%.2f");

      static bool wireframe = waveRenderer->UsesWireframe();
      if (ImGui::Checkbox("Render Wireframe (T)", &wireframe))
        waveRenderer->UseWireframe(wireframe);
    }
  }
  ImGui::End();
  uiRenderer->End();

  // Handle this outside of window so it works when closed.
  if (Vision::Input::KeyPress(SDL_SCANCODE_T))
    waveRenderer->ToggleWireframe();
}

void WaveApp::OnResize(float width, float height)
{
  waveRenderer->Resize(width, height);
}

} // namespace Waves
