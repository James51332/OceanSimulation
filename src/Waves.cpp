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
    generators.push_back(new Generator(renderDevice, fftCalculator));

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
    generator->CalculateOcean(timestep, true);

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
  if (ImGui::Begin("TextureViewer"))
  {
    ImGui::Image((ImTextureID)generators[0]->GetHeightMap(), {400.0f, 400.0f});
    ImGui::Image((ImTextureID)generators[0]->GetNormalMap(), {400.0f, 400.0f});
  }
  ImGui::End();

  if (ImGui::Begin("Ocean Settings"))
  {
    Generator::OceanSettings& settings = generators[0]->GetOceanSettings();
    ImGui::DragFloat2("Wind Velocity", &settings.windVelocity[0], 0.25f);
    ImGui::DragFloat("Gravity", &settings.gravity, 0.05f);
    ImGui::DragFloat("Scale", &settings.scale, 0.0005f);
  }
  ImGui::End();

  uiRenderer->End();
}

void WaveApp::OnResize(float width, float height)
{
  waveRenderer->Resize(width, height);
}

} // namespace Waves
