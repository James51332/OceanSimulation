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
    GeneratorSettings& settings = generator->GetOceanSettings();

    // Set the size of the plane based on increasing prime sizes to prevent tiling.
    static float primeFactors[] = {5.0, 17.0, 101.0};
    settings.planeSize = primeFactors[i];

    // Enable bound wavelength to prevent frequencies from adding themselves twice.
    settings.boundWavelength = 1;

    // Each wavelength should be on the smallest plane that it fits on for the most detail.
    settings.wavelengthMax = settings.planeSize / 2.0;
    settings.wavelengthMin = (i == 0) ? 0.0 : primeFactors[i - 1] / 2.0;

    // Add our generator to the array.
    generators.push_back(generator);
  }

  // Create our RenderPass
  Vision::RenderPassDesc rpDesc;
  rpDesc.Framebuffer = 0;
  rpDesc.LoadOp = Vision::LoadOp::Load;
  rpDesc.StoreOp = Vision::StoreOp::Store;
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
  // Press Esc to close the app
  if (Vision::Input::KeyPress(SDL_SCANCODE_ESCAPE))
  {
    Stop();
    return;
  }

  waveRenderer->UpdateCamera(timestep);

  // Press R to reload shaders
  if (Vision::Input::KeyPress(SDL_SCANCODE_R))
  {
    // We only need to reload one shader since the generators share a pipeline state.
    generators[0]->LoadShaders(true);
    waveRenderer->LoadShaders();
  }

  // If our application is occluded, we skip the rendering to avoid flooding memory with commands
  // that won't be executed.
  if (!ShouldRender())
    return;

  // Begin recording commands
  renderDevice->BeginCommandBuffer();

  if (Vision::Input::KeyDown(SDL_SCANCODE_Q))
    timestep = 0.0f;

  // First, we do the waves pass
  for (auto* generator : generators)
    generator->CalculateOcean(timestep, updateSpectrum);

  // If we have updated our ocean spectrum, we don't need to again until it's changed.
  // updateSpectrum = false;

  // Then we do our the render pass
  waveRenderer->Render(generators);

  // Then we do our UI pass.
  renderDevice->BeginRenderPass(renderPass);
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

  // Handle this outside of window so it works when closed or hidden.
  if (Vision::Input::KeyPress(SDL_SCANCODE_T))
    waveRenderer->ToggleWireframe();

  // Toggle the UI if we don't want to show it.
  static bool showUI = true;
  static bool toggled = false;
  if (Vision::Input::KeyDown(SDL_SCANCODE_LCTRL) && Vision::Input::KeyDown(SDL_SCANCODE_H))
  {
    // Only toggle once until we let go of one of the keys.
    if (!toggled)
      showUI = !showUI;

    toggled = true;
  }
  else
  {
    toggled = false;
  }

  if (!showUI)
    return;

  uiRenderer->Begin();
  if (ImGui::Begin("Control Panel (L Ctrl + H to toggle)"))
  {
    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
    {
      // Print the FPS and frame time
      ImGui::Text("FPS: %.1f", (1000.0f / weightedFrameTime));
      ImGui::Text("Frame Time: %.1fms", weightedFrameTime);

      bool first = true;
      for (auto& generator : generators)
      {
        if (!first)
          ImGui::SameLine();
        else
          first = false;
        ImGui::Image((ImTextureID)generator->GetHeightMap(), {100.0f, 100.0f});
      }
    }

    // Simulation Settings
    if (ImGui::CollapsingHeader("Simulation"))
    {
      int i = 0;
      static const char* text[] = {"Sim 1", "Sim 2", "Sim 3"};
      for (auto& generator : generators)
      {
        ImGui::PushID(i);
        ImGui::TreePush(text[i]);
        if (ImGui::CollapsingHeader(text[i]))
        {
          GeneratorSettings& settings = generator->GetOceanSettings();

          bool us = updateSpectrum;
          us |= ImGui::DragFloat("Wind Speed", &settings.U_10, 0.25f, 1.0f, 100.0f, "%.2f");
          us |= ImGui::DragFloat("Wind Angle", &settings.theta_0, 0.5f, -180.0f, 180.0f, "%.1f");
          us |= ImGui::DragFloat("Gravity", &settings.g, 0.05f, 1.0f, 20.0f, "%.2f");
          us |= ImGui::DragFloat("Scale", &settings.scale, 0.05f, 0.00f, 5.0f, "%.2f");
          us |= ImGui::DragFloat("Displacement", &settings.displacement, 0.01f, 0.0f, 2.0f, "%.2f");
          us |= ImGui::DragFloat("Swell", &settings.swell, 0.005f, 0.0f, 1.0f);
          us |= ImGui::DragFloat("Spread", &settings.spread, 0.01f, 0.0f, 1.0f);
          us |= ImGui::DragFloat("Depth", &settings.h, 0.5f, 15.0f, 500.0f);
          us |= ImGui::DragFloat("Fetch", &settings.F, 1000.0f, 1000.0f, 1000000.0f);
          us |= ImGui::DragFloat("Size", &settings.planeSize, 0.5f, 1.0f, 200.0f, "%.1f");
          updateSpectrum = us;
        }
        ImGui::TreePop();
        ImGui::PopID();
        i++;
      }

      if (updateSpectrum)
      {
        for (int i = 0; i < waveRenderer->GetNumRequiredGenerators(); i++)
        {
          GeneratorSettings& settings = generators[i]->GetOceanSettings();

          // Each wavelength should be on the smallest plane that it fits on for the most detail.
          settings.wavelengthMax = settings.planeSize / 2.0;
          settings.wavelengthMin =
              (i == 0) ? 0.0 : generators[i - 1]->GetOceanSettings().planeSize / 2.0;
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
      ImGui::DragFloat("Fog Start", &data.fogBegin, 1.0f, 0.0f, 400.0f, "%.0f");

      static bool wireframe = waveRenderer->UsesWireframe();
      if (ImGui::Checkbox("Render Wireframe (T)", &wireframe))
        waveRenderer->UseWireframe(wireframe);
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
