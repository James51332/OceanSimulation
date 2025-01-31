#include "Renderer.h"

#include "core/Input.h"

#include "renderer/MeshGenerator.h"
#include "renderer/shader/ShaderCompiler.h"

namespace Waves
{

WaveRenderer::WaveRenderer(Vision::RenderDevice* device, Vision::Renderer* render, float w, float h)
  : renderDevice(device), renderer(render), width(w), height(h)
{
  camera = new Vision::PerspectiveCamera(width, height, 0.1f, 1000.0f);
  camera->SetPosition({0.0f, 2.0f, 0.0f});
  camera->SetRotation({0.0f, -135.0f, 0.0f});

  planeMesh = Vision::MeshGenerator::CreatePlaneMesh(40.0f, 40.0f, 1024, 1024, true);
  cubeMesh = Vision::MeshGenerator::CreateCubeMesh(1.0f);

  GeneratePipelines();
  GenerateBuffers();
}

WaveRenderer::~WaveRenderer()
{
  // Destroy all resources
  renderDevice->DestroyPipeline(wavePS);
  renderDevice->DestroyPipeline(transparentPS);
  renderDevice->DestroyPipeline(skyboxPS);
  renderDevice->DestroyBuffer(wavesBuffer);

  delete planeMesh;
  delete cubeMesh;
  delete camera;
}

void WaveRenderer::UpdateCamera(float timestep)
{
  camera->Update(timestep);
}

void WaveRenderer::Render(std::vector<Generator*>& generators)
{
  // This method requires exactly three simulated oceans to work.
  assert(generators.size() == 3);

  renderer->Begin(camera);

  // Let the GPU know how to access the proper textures
  for (int i = 0; i < generators.size(); i++)
  {
    renderDevice->BindTexture2D(generators[i]->GetHeightMap(), i);

    // This is gonna cause us trouble because we cannot actually simply add normals. We must add
    // slope vectors and then cross multiply to get our final normal. So we are gonna have to do
    // some restructing to get this to work. Maybe it will save us some math though in the end.
    renderDevice->BindTexture2D(generators[i]->GetSlopeMap(), i + 3);

    // Displacement is closely related to slope as well, so we can perhaps just send the slope
    // texture to the renderer and calculate the displacement and normal in the vertex shader.
    renderDevice->BindTexture2D(generators[i]->GetDisplacementMap(), i + 6);

    // Update our ocean buffer data.
    wavesBufferData.planeSize[i] = generators[i]->GetOceanSettings().planeSize;
  }

  // Set and bind our UBOs.
  renderDevice->SetBufferData(wavesBuffer, &wavesBufferData, sizeof(WaveRenderData));
  renderDevice->BindBuffer(wavesBuffer, 1);

  // Draw the ocean in wireframe mode if we press T.
  if (!Vision::Input::KeyDown(SDL_SCANCODE_T))
    renderer->DrawMesh(planeMesh, wavePS);
  else
    renderer->DrawMesh(planeMesh, transparentPS);

  // Draw the skybox wherever the ocean hasn't written to the depthbuffer.
  renderer->DrawMesh(cubeMesh, skyboxPS);

  renderer->End();
}

void WaveRenderer::Resize(float w, float h)
{
  camera->SetWindowSize(w, h);
  width = w;
  height = h;
}

void WaveRenderer::LoadShaders()
{
  if (wavePS)
  {
    renderDevice->DestroyPipeline(wavePS);
    renderDevice->DestroyPipeline(transparentPS);
    renderDevice->DestroyPipeline(skyboxPS);
  }

  GeneratePipelines();
}

void WaveRenderer::GeneratePipelines()
{
  // Create the shaders by loading from disk and compiling
  Vision::ShaderCompiler compiler;
  std::unordered_map<std::string, Vision::ShaderSPIRV> waveShaders =
      compiler.CompileFileToMap("resources/waveShader.glsl");

  // Create our wave pipeline state
  {
    Vision::RenderPipelineDesc psDesc;
    psDesc.VertexShader = waveShaders["waveVertex"];
    psDesc.PixelShader = waveShaders["waveFragment"];
    psDesc.Layouts = {
        Vision::BufferLayout({{Vision::ShaderDataType::Float3, "Position"},
                              {Vision::ShaderDataType::Float3, "Normal"},
                              {Vision::ShaderDataType::Float4, "Color"},
                              {Vision::ShaderDataType::Float2, "UV"}}
        )
    };
    wavePS = renderDevice->CreateRenderPipeline(psDesc);

    // Create a second pipeline state for rendering the mesh of our water.
    psDesc.FillMode = Vision::GeometryFillMode::Line;
    transparentPS = renderDevice->CreateRenderPipeline(psDesc);
  }

  // Create our skybox pipelines state
  {
    Vision::RenderPipelineDesc psDesc;
    psDesc.VertexShader = waveShaders["skyVertex"];
    psDesc.PixelShader = waveShaders["skyFragment"];
    psDesc.Layouts = {
        Vision::BufferLayout({{Vision::ShaderDataType::Float3, "Position"},
                              {Vision::ShaderDataType::Float3, "Normal"},
                              {Vision::ShaderDataType::Float4, "Color"},
                              {Vision::ShaderDataType::Float2, "UV"}}
        )
    };

    // We set the depth of all fragments to 1, so only pixels that weren't
    // previously rendered to will be written by the depth map, but we do in
    // fact want to discard the initial pixels with a depth of 1.
    psDesc.DepthFunc = Vision::DepthFunc::LessEqual;
    // TODO: psDesc.DepthTest = true;

    skyboxPS = renderDevice->CreateRenderPipeline(psDesc);
  }
}

void WaveRenderer::GenerateBuffers()
{
  // Fill our wave buffer in with meaningful data.
  wavesBufferData.planeSize[0] = 40.0f;
  wavesBufferData.planeSize[1] = 200.0f;
  wavesBufferData.planeSize[2] = 1000.0f;
  wavesBufferData.skyColor = glm::vec4(0.53f, 0.8f, 0.94f, 1.0f);
  wavesBufferData.scatterColor = glm::vec4(0.53f, 0.8f, 0.94f, 1.0f);
  wavesBufferData.waveColor = glm::vec4(0.0f, 0.33f, 0.47f, 1.0f);
  wavesBufferData.sunColor = glm::vec4(1.0f, 0.9f, 0.5f, 1.0f);
  wavesBufferData.lightDirection = glm::normalize(glm::vec3(10.0f, 1.5f, 10.0f));
  wavesBufferData.sunViewAngle = 2.0f;
  wavesBufferData.sunFalloffAngle = 1.0f;
  wavesBufferData.cameraFOV = camera->GetFOV();

  Vision::BufferDesc bufferDesc;
  bufferDesc.Type = Vision::BufferType::Uniform;
  bufferDesc.Usage = Vision::BufferUsage::Dynamic;
  bufferDesc.Size = sizeof(WaveRenderData);
  bufferDesc.Data = &wavesBufferData;
  bufferDesc.DebugName = "Wave Renderer Buffer";
  wavesBuffer = renderDevice->CreateBuffer(bufferDesc);
}

} // namespace Waves
