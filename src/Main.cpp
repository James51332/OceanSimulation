#include "core/App.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

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

  Wave wave;
  Vision::Buffer* waveBuffer;

  Waves()
  {
    renderer = new Vision::Renderer(m_DisplayWidth, m_DisplayHeight, m_DisplayScale);
    camera = new Vision::PerspectiveCamera(m_DisplayWidth, m_DisplayHeight);

    planeMesh = Vision::MeshGenerator::CreatePlaneMesh(10.0f, 10.0f, 50, 50);
    waveShader = new Vision::Shader("resources/waveShader.glsl");

    // generate wave (manually for now)
    glm::vec2 origin = { 0.0f, 0.0f };
    glm::vec2 direction = glm::circularRand(1.0f);
    float amplitude = 1.0f;
    float wavelength = 2.0f;
    float angularFrequency = 10.0f;
    float phase = 0.0f;
    wave.OriginDir = { origin, direction };
    wave.Scale = { amplitude, wavelength, angularFrequency, phase };

    // upload to shader
    Vision::BufferDesc desc;
    desc.Type = GL_UNIFORM_BUFFER;
    desc.Usage = GL_STATIC_DRAW;
    desc.Size = sizeof(Wave);
    desc.Data = &wave;
    waveBuffer = new Vision::Buffer(desc);

    waveShader->Use();
    waveShader->SetUniformBlock(waveBuffer, "WaveProperties", 0);
  }

  ~Waves()
  {
    delete renderer;
    delete camera;

    delete planeMesh;
    delete waveShader;
    delete waveBuffer;
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