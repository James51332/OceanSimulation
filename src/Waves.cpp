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
  generator = new Generator(renderDevice);
  waveRenderer = new WaveRenderer(renderDevice, renderer, GetDisplayWidth(), GetDisplayHeight());

  // Create our RenderPass
  Vision::RenderPassDesc rpDesc;
  rpDesc.Framebuffer = 0;
  rpDesc.LoadOp = Vision::LoadOp::Clear;
  rpDesc.StoreOp = Vision::StoreOp::Store;
  renderPass = renderDevice->CreateRenderPass(rpDesc);
}

WaveApp::~WaveApp()
{
  renderDevice->DestroyRenderPass(renderPass);

  delete waveRenderer;
  delete generator;
}

void WaveApp::OnUpdate(float timestep)
{
  waveRenderer->UpdateCamera(timestep);

  if (Vision::Input::KeyPress(SDL_SCANCODE_R)) // reload shaders
    generator->LoadShaders();

  if (!ShouldRender())
    return;

  // Begin recording commands
  renderDevice->BeginCommandBuffer();

  // First we do the waves pass
  generator->GenerateSpectrum(timestep);

  // Add a barrier to make image memory accessible and visible.
  renderDevice->ImageBarrier();
  generator->GenerateWaves();
  renderDevice->ImageBarrier();

  // Then we do our the render pass
  renderDevice->BeginRenderPass(renderPass);
  waveRenderer->Render(generator->GetHeightMap(), 0);
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
      ImGui::Image((ImTextureID)generator->GetHeightMap(), {400.0f, 400.0f});
      ImGui::Image((ImTextureID)generator->GetNormalMap(), {400.0f, 400.0f});
    }
    ImGui::End();
    uiRenderer->End();
  }

void WaveApp::OnResize(float width, float height)
{
  waveRenderer->Resize(width, height);
}

}
