#include "Renderer.h"

#include "renderer/MeshGenerator.h"
#include "renderer/shader/ShaderCompiler.h"

namespace Waves
{

WaveRenderer::WaveRenderer(Vision::RenderDevice* device, Vision::Renderer* render, float w, float h)
  : renderDevice(device), renderer(render), width(w), height(h)
{
  camera = new Vision::PerspectiveCamera(width, height);

  planeMesh = Vision::MeshGenerator::CreatePlaneMesh(50.0f, 50.0f, 1000, 1000, true);
  cubeMesh = Vision::MeshGenerator::CreateCubeMesh(1.0f);

  GeneratePipelines();
  GenerateTextures();
  GenerateBuffers();
}

WaveRenderer::~WaveRenderer()
{
  // Destory all resources
  renderDevice->DestroyCubemap(skyboxTexture);

  renderDevice->DestroyPipeline(wavePS);
  renderDevice->DestroyPipeline(skyboxPS);

  delete planeMesh;
  delete cubeMesh;
  delete camera;
}

void WaveRenderer::UpdateCamera(float timestep)
{
  camera->Update(timestep);
}

void WaveRenderer::Render(ID heightMap, ID normalMap)
{
  renderer->Begin(camera);

  renderDevice->BindTexture2D(heightMap);
  renderer->DrawMesh(planeMesh, wavePS);

  renderDevice->BindCubemap(skyboxTexture);
  renderer->DrawMesh(cubeMesh, skyboxPS);

  renderer->End();
}

void WaveRenderer::Resize(float w, float h)
{
  camera->SetWindowSize(w, h);
  width = w;
  height = h;
}

void WaveRenderer::GeneratePipelines()
{
  // Create the shaders by loading from disk and compiling
  Vision::ShaderCompiler compiler;
  std::vector<Vision::ShaderSPIRV> waveShaders = compiler.CompileFile("resources/waveShader.glsl");
  std::vector<Vision::ShaderSPIRV> skyboxShaders =  compiler.CompileFile("resources/skyboxShader.glsl");

  // Create our wave pipeline state
  {
    Vision::RenderPipelineDesc psDesc;
    psDesc.VertexShader = waveShaders[0];
    psDesc.PixelShader = waveShaders[1];
    psDesc.Layouts = { Vision::BufferLayout({
      { Vision::ShaderDataType::Float3, "Position"},
      { Vision::ShaderDataType::Float3, "Normal" },
      { Vision::ShaderDataType::Float4, "Color" },
      { Vision::ShaderDataType::Float2, "UV" }}) 
    };
    wavePS = renderDevice->CreateRenderPipeline(psDesc);
  }

  // Create our skybox pipelines state
  {
    Vision::RenderPipelineDesc psDesc;
    psDesc.VertexShader = skyboxShaders[0];
    psDesc.PixelShader = skyboxShaders[1];
    psDesc.Layouts = { Vision::BufferLayout({
      { Vision::ShaderDataType::Float3, "Position"},
      { Vision::ShaderDataType::Float3, "Normal" },
      { Vision::ShaderDataType::Float4, "Color" },
      { Vision::ShaderDataType::Float2, "UV" }}) 
    };
    
    // We set the depth of all fragments to 1, so only pixels that weren't
    // previously rendered to will be written by the depth map, but we do in fact
    // want to discard the initial pixels with a depth of 1.
    psDesc.DepthFunc = Vision::DepthFunc::LessEqual; 
    //psDesc.DepthTest = true;
    
    skyboxPS = renderDevice->CreateRenderPipeline(psDesc);
  }
}

void WaveRenderer::GenerateTextures()
{
  Vision::CubemapDesc cubemapDesc;
  cubemapDesc.Textures = {
    "resources/skybox/bluecloud_ft.jpg",
    "resources/skybox/bluecloud_bk.jpg",
    "resources/skybox/bluecloud_up.jpg",
    "resources/skybox/bluecloud_dn.jpg",
    "resources/skybox/bluecloud_rt.jpg",
    "resources/skybox/bluecloud_lf.jpg"
  };
  skyboxTexture = renderDevice->CreateCubemap(cubemapDesc);
}

void WaveRenderer::GenerateBuffers()
{

}


}
