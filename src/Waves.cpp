#include "Waves.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

#include <imgui.h>

#include "core/Input.h"

namespace Waves 
{

struct Wave
{
  // Origin & Direction
  glm::vec2 Origin;
  glm::vec2 Direction;

  // Other Properties
  float Amplitude;
  float Wavelength;
  float AngularFrequency;
  float Phase;
};

WaveApp::WaveApp()
{
  generator = new Generator(renderDevice);
  waveRenderer = new WaveRenderer(renderDevice, renderer, displayWidth, displayHeight);

  // Create our RenderPass
  Vision::RenderPassDesc rpDesc;
  rpDesc.Framebuffer = 0;
  rpDesc.LoadOp = Vision::LoadOp::Clear;
  rpDesc.StoreOp = Vision::StoreOp::Store;
  renderPass = renderDevice->CreateRenderPass(rpDesc);

  generator->GenerateWaves(0.0167f);
  Stop();
}

WaveApp::~WaveApp()
{
  renderDevice->DestroyRenderPass(renderPass);

  delete waveRenderer;
  delete generator;
}

void WaveApp::GenerateWaves()
{
  // float curFreq = angularFrequency;
  // float curAmp = amplitude;
  // float curLength = wavelength;

  // for (int i = 0; i < 10; i++)
  // {
  //   waves[i].Origin = glm::linearRand(glm::vec2(-1.0f), glm::vec2(1.0f));
  //   waves[i].Direction = glm::circularRand(1.0f);
  //   waves[i].Phase = glm::linearRand(0.0f, 6.28f);
  //   waves[i].AngularFrequency = glm::sqrt(9.8f * 2.0f * M_PI / curLength); // dispersion relation
  //   waves[i].Amplitude = curAmp;
  //   waves[i].Wavelength = curLength;

  //   curFreq *= freqDamp;
  //   curAmp *= ampDamp;
  //   curLength *= lengthDamp;
  // }

  // renderDevice->SetBufferData(waveBuffer, waves, sizeof(Wave) * 10);
}

void WaveApp::OnUpdate(float timestep)
{
  // waveRenderer->UpdateCamera(timestep);

  // // Begin recording commands
  // renderDevice->BeginCommandBuffer();

  // // First we do the waves pass
  // if (Vision::Input::KeyPress(SDL_SCANCODE_T))
  // {
  //   renderDevice->BeginComputePass();
  //   generator->GenerateWaves(timestep);
  // }

  // // Add a barrier to make image memory accessible and visible.
  // renderDevice->ImageBarrier();

  // // Then we do our the render pass
  // renderDevice->BeginRenderPass(renderPass);
  // waveRenderer->Render(0, 0);
  // DrawUI();
  // renderDevice->EndRenderPass();

  // // And present to the the screen
  // renderDevice->SchedulePresentation();
  // renderDevice->SubmitCommandBuffer();
}

void WaveApp::DrawUI()
{
    uiRenderer->Begin();
    if (ImGui::Begin("Settings"))
    {
      ImGui::PushItemWidth(0.4f * ImGui::GetWindowWidth());
      bool changed = false;
      changed |= ImGui::DragFloat("Wavelength", &wavelength, 0.003f);
      changed |= ImGui::DragFloat("Amplitude", &amplitude, 0.003f);
      changed |= ImGui::DragFloat("Frequency", &angularFrequency, 0.003f);
      changed |= ImGui::DragFloat("Freq. Dampening", &freqDamp, 0.003f);
      changed |= ImGui::DragFloat("Length Dampening", &lengthDamp, 0.003f);
      changed |= ImGui::DragFloat("Amp. Damp", &ampDamp, 0.003f);

      // ImGui::ColorEdit4("Water Color", &lightInfo.waveColor[0]);
      // ImGui::DragFloat3("Light Pos", &lightInfo.lightPos[0]);

      if (changed)
        GenerateWaves();

      ImGui::Image((ImTextureID)generator->GetHeightMap(), {200.0f, 200.0f});

      ImGui::PopItemWidth();
    }
    ImGui::End();
    uiRenderer->End();
  }

void WaveApp::OnResize(float width, float height)
{
  waveRenderer->Resize(width, height);
}

}