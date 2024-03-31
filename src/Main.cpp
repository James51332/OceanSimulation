#include "core/App.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "renderer/Renderer.h"
#include "renderer/MeshGenerator.h"

struct Wave
{
  // Origin (x, y); Direction (z, w)
  glm::vec4 OriginDir;

  // Amplitude: (x); Wavelength (y); Angular Frequency (z); Phase (w)
  glm::vec4 Scale;
};

struct Waves : public Vision::App
{
  Vision::Renderer* renderer;
  Vision::PerspectiveCamera* camera;
  Vision::Mesh* planeMesh;
  Vision::Shader* waveShader;

  Waves()
  {
    renderer = new Vision::Renderer(m_DisplayWidth, m_DisplayHeight, m_DisplayScale);
    camera = new Vision::PerspectiveCamera(m_DisplayWidth, m_DisplayHeight);

    planeMesh = Vision::MeshGenerator::CreatePlaneMesh(10.0f, 10.0f, 50, 50);
    waveShader = new Vision::Shader("resources/waveShader.glsl");
  }

  ~Waves()
  {
    delete renderer;
    delete camera;

    delete planeMesh;
    delete waveShader;
  }

  void OnUpdate(float timestep)
  {
    camera->Update(timestep);

    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderer->Begin(camera);
    renderer->DrawMesh(planeMesh, waveShader);
    renderer->End();
  }

  void OnResize()
  {
    renderer->Resize(m_DisplayWidth, m_DisplayHeight);
  }
};

int main()
{
  Waves* waves = new Waves();
  waves->Run();
  delete waves;
}