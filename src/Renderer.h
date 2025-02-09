#pragma once

#include <vector>

#include "renderer/RenderDevice.h"
#include "renderer/Renderer.h"
#include "renderer/Renderer2D.h"

#include "Generator.h"

namespace Waves
{

// Wave Uniform Buffer Data Structure
struct WaveRenderData
{
  // Simulation Data
  glm::vec4 planeSize = glm::vec4(0.0f); // The size of the three planes that make up our water
  glm::vec4 displacementScale = glm::vec4(0.0f); // The displacement scale for each plane.

  // Rendering Data
  glm::vec4 waveColor = glm::vec4(0.0f, 0.33f, 0.47f, 1.0f); // The color of the wave
  glm::vec4 scatterColor = glm::vec4(0.5f, .8f, .9f, 1.0f);  // Scatter color of the water
  glm::vec4 skyColor = glm::vec4(0.53f, 0.8f, 0.94f, 1.0f);  // Color of the sky
  glm::vec4 sunColor = glm::vec4(1.0f);                      // Color of sun and horizon
  glm::vec3 lightDirection = glm::vec3(.703f, .105f, .703f); // Direction towards the sun
  float sunViewAngle = 3.0f;                                 // The angle of the sun in the sky
  float sunFalloffAngle = 1.0f;                              // The angle between hard edge and sky
  float fogBegin = 30.0f;                                    // Where the fog begins
  float cameraNear = 20.0f;                                  // The near camera clipping plane
  float cameraFar = 50.0f;                                   // The far camera clipping plane
};

class WaveRenderer
{
  using ID = Vision::ID;

public:
  WaveRenderer(Vision::RenderDevice* renderDevice, Vision::Renderer* renderer, float width,
               float height);
  ~WaveRenderer();

  void UpdateCamera(float timestep);

  // Requires that the generators is an array of three valid FFT oceans
  void Render(std::vector<Generator*>& generators);
  static std::size_t GetNumRequiredGenerators() { return 3; }

  void Resize(float width, float height);

  void UseWireframe(bool wireframe = true) { useWireframe = wireframe; }
  void ToggleWireframe() { useWireframe = !useWireframe; }
  bool UsesWireframe() const { return useWireframe; }

  void LoadShaders();

  WaveRenderData& GetWaveRenderData() { return wavesBufferData; }

private:
  void GeneratePasses();
  void GeneratePipelines();
  void GenerateBuffers();

private:
  // General Rendering Data
  Vision::RenderDevice* renderDevice = nullptr;
  Vision::Renderer* renderer = nullptr;
  float width = 0.0f, height = 0.0f;
  Vision::PerspectiveCamera* camera = nullptr;
  ID wavePass = 0, skyboxPass = 0, postPass = 0;

  // Rendering Framebuffer (used for post processing)
  ID framebuffer = 0, fbColor = 0, fbDepth = 0;
  ID skyboxBuffer = 0, sbColor = 0;

  // Meshes
  Vision::Mesh* planeMesh = nullptr; // surface of water
  Vision::Mesh* cubeMesh = nullptr;  // skybox and light visualization
  Vision::Mesh* quadMesh = nullptr;  // quad that covers screen for rendering fb

  // Pipelines and Shaders
  bool useWireframe = false;
  ID wavePS = 0, wireframePS = 0, skyboxPS = 0, postPS = 0;

  // Wave Uniform Buffer
  WaveRenderData wavesBufferData;
  ID wavesBuffer = 0;
};

} // namespace Waves