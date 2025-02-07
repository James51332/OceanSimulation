#include "Renderer.h"

#include "core/Input.h"

#include "renderer/MeshGenerator.h"
#include "renderer/shader/ShaderCompiler.h"

namespace Waves
{

WaveRenderer::WaveRenderer(Vision::RenderDevice* device, Vision::Renderer* render, float w, float h)
  : renderDevice(device), renderer(render), width(w), height(h)
{
  camera = new Vision::PerspectiveCamera(width, height, 1.0f, 1500.0f);
  camera->SetPosition({0.0f, 5.0f, 0.0f});
  camera->SetRotation({-5.0f, -135.0f, 0.0f});

  planeMesh = Vision::MeshGenerator::CreatePlaneMesh(40.0f, 40.0f, 1024, 1024, true);
  cubeMesh = Vision::MeshGenerator::CreateCubeMesh(1.0f);
  quadMesh = Vision::MeshGenerator::CreatePlaneMesh(2.0f, 2.0f, 1, 1);

  GeneratePasses();
  GeneratePipelines();
  GenerateBuffers();
}

WaveRenderer::~WaveRenderer()
{
  // Destroy all resources
  renderDevice->DestroyPipeline(wavePS);
  renderDevice->DestroyPipeline(wireframePS);
  renderDevice->DestroyPipeline(skyboxPS);
  renderDevice->DestroyPipeline(postPS);
  renderDevice->DestroyBuffer(wavesBuffer);
  renderDevice->DestroyFramebuffer(framebuffer);
  renderDevice->DestroyFramebuffer(skyboxBuffer);
  renderDevice->DestroyRenderPass(wavePass);
  renderDevice->DestroyRenderPass(skyboxPass);
  renderDevice->DestroyRenderPass(postPass);

  delete planeMesh;
  delete cubeMesh;
  delete quadMesh;
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

  // First, we perform our pass that renders to the framebuffer
  renderDevice->BeginRenderPass(wavePass);
  renderer->Begin(camera);

  // Let the GPU know how to access the proper textures
  for (int i = 0; i < generators.size(); i++)
  {
    renderDevice->BindTexture2D(generators[i]->GetHeightMap(), i);
    renderDevice->BindTexture2D(generators[i]->GetDisplacementMap(), i + 3);
    renderDevice->BindTexture2D(generators[i]->GetJacobianMap(), i + 6);
    // Update our ocean buffer data.
    wavesBufferData.planeSize[i] = generators[i]->GetOceanSettings().planeSize;
  }

  // Set the camera clipping planes and displacement scale.
  wavesBufferData.displacementScale = generators[0]->GetOceanSettings().displacement;
  wavesBufferData.cameraNear = camera->GetNear();
  wavesBufferData.cameraFar = camera->GetFar();

  // Set and bind our UBOs.
  renderDevice->SetBufferData(wavesBuffer, &wavesBufferData, sizeof(WaveRenderData));
  renderDevice->BindBuffer(wavesBuffer, 1);

  // Choose the correct pipeline based on whether or not we want to use wireframe mode.
  if (useWireframe)
    renderer->DrawMesh(planeMesh, wireframePS);
  else
    renderer->DrawMesh(planeMesh, wavePS);

  // Render the skybox so that when we mix to create fog, we don't mix with the clear color.
  renderer->DrawMesh(cubeMesh, skyboxPS);

  // Now, we switch framebuffers and render the skybox again to a different framebuffer.
  renderDevice->EndRenderPass();
  renderDevice->BeginRenderPass(skyboxPass);

  renderDevice->BindBuffer(wavesBuffer, 1);
  renderer->DrawMesh(cubeMesh, skyboxPS);

  renderer->End();
  renderDevice->EndRenderPass();

  // Now, we perform our pass to the screen using our quad.
  renderDevice->BeginRenderPass(postPass);

  // Submit our framebuffer textures.
  renderDevice->BindTexture2D(fbColor, 9);
  renderDevice->BindTexture2D(fbDepth, 10);
  renderDevice->BindTexture2D(sbColor, 11);

  // Perform a pass without a camera to allow us to render the quad.
  renderer->Begin(nullptr);

  renderDevice->BindBuffer(wavesBuffer, 1);
  renderer->DrawMesh(quadMesh, postPS);
  renderer->End();

  // Now, we are done!
  renderDevice->EndRenderPass();
}

void WaveRenderer::Resize(float w, float h)
{
  camera->SetWindowSize(w, h);
  renderDevice->ResizeFramebuffer(framebuffer, w, h);
  renderDevice->ResizeFramebuffer(skyboxBuffer, w, h);
  width = w;
  height = h;
}

void WaveRenderer::LoadShaders()
{
  if (wavePS)
  {
    renderDevice->DestroyPipeline(wavePS);
    renderDevice->DestroyPipeline(wireframePS);
    renderDevice->DestroyPipeline(skyboxPS);
  }

  GeneratePipelines();
}

void WaveRenderer::GeneratePasses()
{
  // Create our framebuffer so that we can render to it.
  Vision::FramebufferDesc fbDesc;
  fbDesc.Width = width;
  fbDesc.Height = height;
  fbDesc.ColorFormat = Vision::PixelType::BGRA8;
  fbDesc.DepthType = Vision::PixelType::Depth32Float;
  framebuffer = renderDevice->CreateFramebuffer(fbDesc);
  fbColor = renderDevice->GetFramebufferColorTex(framebuffer);
  fbDepth = renderDevice->GetFramebufferDepthTex(framebuffer);

  // Render the skybox to its own framebuffer and merge.
  skyboxBuffer = renderDevice->CreateFramebuffer(fbDesc);
  sbColor = renderDevice->GetFramebufferColorTex(skyboxBuffer);

  // Then we create the pass that renders to our framebuffer.
  Vision::RenderPassDesc desc;
  desc.LoadOp = Vision::LoadOp::Clear;
  desc.StoreOp = Vision::StoreOp::Store;
  desc.Framebuffer = framebuffer;
  wavePass = renderDevice->CreateRenderPass(desc);

  // Here we create our buffer for the skybox
  desc.Framebuffer = skyboxBuffer;
  skyboxPass = renderDevice->CreateRenderPass(desc);

  // Now we create the pass that renders to the screen buffer.
  desc.Framebuffer = 0;
  postPass = renderDevice->CreateRenderPass(desc);
}

void WaveRenderer::GeneratePipelines()
{
  // Create the shaders by loading from disk and compiling
  Vision::ShaderCompiler compiler;
  std::unordered_map<std::string, Vision::ShaderSPIRV> waveShaders =
      compiler.CompileFileToMap("resources/waveShader.glsl", true);

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
    wireframePS = renderDevice->CreateRenderPipeline(psDesc);
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

    skyboxPS = renderDevice->CreateRenderPipeline(psDesc);
  }

  // Create our post processing pipeline
  {
    Vision::RenderPipelineDesc psDesc;
    psDesc.VertexShader = waveShaders["postVertex"];
    psDesc.PixelShader = waveShaders["postFragment"];
    psDesc.Layouts = {
        Vision::BufferLayout({{Vision::ShaderDataType::Float3, "Position"},
                              {Vision::ShaderDataType::Float3, "Normal"},
                              {Vision::ShaderDataType::Float4, "Color"},
                              {Vision::ShaderDataType::Float2, "UV"}}
        )
    };

    postPS = renderDevice->CreateRenderPipeline(psDesc);
  }
}

void WaveRenderer::GenerateBuffers()
{
  // Fill our wave buffer in with meaningful data.
  wavesBufferData.skyColor = glm::vec4(0.53f, 0.8f, 0.94f, 1.0f);
  wavesBufferData.scatterColor = glm::vec4(0.53f, 0.8f, 0.94f, 1.0f);
  wavesBufferData.waveColor = glm::vec4(0.0f, 0.33f, 0.47f, 1.0f);
  wavesBufferData.sunColor = glm::vec4(1.0f, 0.9f, 0.5f, 1.0f);
  wavesBufferData.lightDirection = glm::normalize(glm::vec3(10.0f, 1.5f, 10.0f));
  wavesBufferData.sunViewAngle = 2.0f;
  wavesBufferData.sunFalloffAngle = 2.0f;

  Vision::BufferDesc bufferDesc;
  bufferDesc.Type = Vision::BufferType::Uniform;
  bufferDesc.Usage = Vision::BufferUsage::Dynamic;
  bufferDesc.Size = sizeof(WaveRenderData);
  bufferDesc.Data = &wavesBufferData;
  bufferDesc.DebugName = "Wave Renderer Buffer";
  wavesBuffer = renderDevice->CreateBuffer(bufferDesc);
}

} // namespace Waves
