#include "core/App.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

#include <imgui.h>

#include "core/Input.h"
#include "renderer/Renderer.h"
#include "renderer/MeshGenerator.h"

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

struct Waves : public Vision::App
{
  Vision::Renderer* renderer;
  Vision::ImGuiRenderer* uiRenderer;
  Vision::PerspectiveCamera* camera;
  Vision::Mesh* planeMesh;
  Vision::Shader* waveShader;

  constexpr static int numWaves = 10;
  Wave waves[numWaves];
  Vision::Buffer* waveBuffer;

  Waves()
  {
    renderer = new Vision::Renderer(m_DisplayWidth, m_DisplayHeight, m_DisplayScale);
    uiRenderer = new Vision::ImGuiRenderer(m_DisplayWidth, m_DisplayHeight, m_DisplayScale);
    camera = new Vision::PerspectiveCamera(m_DisplayWidth, m_DisplayHeight);

    planeMesh = Vision::MeshGenerator::CreatePlaneMesh(20.0f, 20.0f, 20, 20, true, false);
    waveShader = new Vision::Shader("resources/waveShader.glsl");

    // generate waves (manually for now)
    srand(time(nullptr));

    // create wave uniform buffers
    Vision::BufferDesc desc;
    desc.Type = GL_UNIFORM_BUFFER;
    desc.Usage = GL_STATIC_DRAW;
    desc.Size = sizeof(Wave) * numWaves;
    desc.Data = nullptr;
    waveBuffer = new Vision::Buffer(desc);

    GenerateWaves();
  }

  ~Waves()
  {
    delete renderer;
    delete camera;

    delete planeMesh;
    delete waveShader;
    delete waveBuffer;
  }

  float wavelength = 6.5f;
  float amplitude = 0.10f;
  float angularFrequency = 1.5f;
  float freqDamp = 1.234f;
  float lengthDamp = 0.85f;
  float ampDamp = 0.75f;

  void GenerateWaves()
  {
    float curFreq = angularFrequency;
    float curAmp = amplitude;
    float curLength = wavelength;

    for (int i = 0; i < 10; i++)
    {
      waves[i].Origin = glm::linearRand(glm::vec2(-1.0f), glm::vec2(1.0f));
      waves[i].Direction = glm::circularRand(1.0f);

      waves[i].Phase = glm::linearRand(0.0f, 6.28f);

      waves[i].AngularFrequency = curFreq;
      waves[i].Amplitude = curAmp;
      waves[i].Wavelength = curLength;

      curFreq *= freqDamp;
      curAmp *= ampDamp;
      curLength *= lengthDamp;
    }

    waveBuffer->SetData(waves, sizeof(Wave) * 10);

    waveShader->Use();
    waveShader->SetUniformBlock(waveBuffer, "WaveProperties", 0);
  }

  void OnUpdate(float timestep)
  {
    camera->Update(timestep);

    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Debug
    if (Vision::Input::KeyDown(SDL_SCANCODE_TAB))
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    renderer->Begin(camera);
    renderer->DrawMesh(planeMesh, waveShader);
    renderer->End();

    DrawUI();
  }

  void DrawUI()
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

      if (changed)
        GenerateWaves();

      ImGui::PopItemWidth();
    }
    ImGui::End();
    uiRenderer->End();
  }

  void OnResize()
  {
    renderer->Resize(m_DisplayWidth, m_DisplayHeight);
    uiRenderer->Resize(m_DisplayWidth, m_DisplayHeight);
  }
};

int main()
{
  Waves* waves = new Waves();
  waves->Run();
  delete waves;
}