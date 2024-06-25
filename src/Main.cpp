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

struct LightInfo
{
  glm::vec4 lightPos;
  glm::vec4 camPos;
  glm::vec4 waveColor;
};

struct Waves : public Vision::App
{
  Vision::PerspectiveCamera* camera;
  Vision::Mesh* planeMesh;
  Vision::ID waveShader, wavePS;

  Vision::Mesh* skyboxMesh;
  Vision::ID skyboxTexture;
  Vision::ID skyboxShader, skyboxPS;
  Vision::ID renderPass;

  constexpr static int numWaves = 10;
  Wave waves[numWaves];
  Vision::ID waveBuffer, lightBuffer;
  LightInfo lightInfo;

  Waves()
  {
    // Create the camera and the meshes
    camera = new Vision::PerspectiveCamera(displayWidth, displayHeight);
    planeMesh = Vision::MeshGenerator::CreatePlaneMesh(10.0f, 10.0f, 200, 200, true);
    skyboxMesh = Vision::MeshGenerator::CreateCubeMesh(1.0f);

    // Load our shaders from disk
    Vision::ShaderDesc shaderDesc;
    {
      shaderDesc.FilePath = "resources/waveShader.glsl";
      waveShader = renderDevice->CreateShader(shaderDesc);

      shaderDesc.FilePath = "resources/skyboxShader.glsl";
      skyboxShader = renderDevice->CreateShader(shaderDesc);
    }
    
    // Generate our cubemap texture
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

    // Create our wave and skybox pipeline states
    {
      Vision::PipelineDesc psDesc;

      // first, configure psDesc for wavePS
      psDesc.Shader = waveShader;
      psDesc.Layouts = 
      { Vision::BufferLayout({
        { Vision::ShaderDataType::Float3, "Position"},
        { Vision::ShaderDataType::Float3, "Normal" },
        { Vision::ShaderDataType::Float4, "Color" },
        { Vision::ShaderDataType::Float2, "UV" }}) 
      };
      wavePS = renderDevice->CreatePipeline(psDesc);

      // then, configure for skyboxPS
      psDesc.Shader = skyboxShader;
      psDesc.DepthTest = true;
      psDesc.DepthFunc = Vision::DepthFunc::LessEqual;
      skyboxPS = renderDevice->CreatePipeline(psDesc);
    }

    // Allocate Uniform Buffer.
    {
      // generate waves (manually for now)
      srand(time(nullptr));

      // create wave uniform buffers
      Vision::BufferDesc desc;
      desc.Type = Vision::BufferType::Uniform;
      desc.Usage = Vision::BufferUsage::Static;
      desc.Size = sizeof(Wave) * numWaves;
      desc.Data = nullptr;
      desc.DebugName = "Wave Data";
      waveBuffer = renderDevice->CreateBuffer(desc);

      GenerateWaves();
    }

    // Create light buffer
    {
      lightInfo.camPos = glm::vec4(camera->GetPosition(), 1.0f);
      lightInfo.lightPos = { 100.0, 5.0f, 100.0f, 1.0f };
      lightInfo.waveColor = { 0.3f, 0.2f, 0.75f, 1.0f };

      Vision::BufferDesc desc;
      desc.Type = Vision::BufferType::Uniform;
      desc.Usage = Vision::BufferUsage::Dynamic;
      desc.Size = sizeof(LightInfo);
      desc.Data = &lightInfo;
      desc.DebugName = "Light Info";
      lightBuffer = renderDevice->CreateBuffer(desc);
    }

    // Create our RenderPass
    {
      Vision::RenderPassDesc rpDesc;
      rpDesc.Framebuffer = 0; // render to the window directly
      rpDesc.LoadOp = Vision::LoadOp::Clear; // the skybox will cover everything
      rpDesc.StoreOp = Vision::StoreOp::Store;
      renderPass = renderDevice->CreateRenderPass(rpDesc);
    }
  }

  ~Waves()
  {
    delete camera;
    delete planeMesh;
    delete skyboxMesh;

    renderDevice->DestroyPipeline(wavePS);
    renderDevice->DestroyPipeline(skyboxPS);

    renderDevice->DestroyShader(waveShader);
    renderDevice->DestroyShader(skyboxShader);

    renderDevice->DestroyCubemap(skyboxTexture);
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

      waves[i].AngularFrequency = glm::sqrt(9.8f * 2.0f * M_PI / curLength); // dispersion relation
      waves[i].Amplitude = curAmp;
      waves[i].Wavelength = curLength;

      curFreq *= freqDamp;
      curAmp *= ampDamp;
      curLength *= lengthDamp;
    }

    renderDevice->SetBufferData(waveBuffer, waves, sizeof(Wave) * 10);
  }

  void OnUpdate(float timestep)
  {
    camera->Update(timestep);

    renderDevice->BeginCommandBuffer();
    renderDevice->BeginRenderPass(renderPass);

    renderer->Begin(camera);
    
    // draw the water
    lightInfo.camPos = glm::vec4(camera->GetPosition(), 1.0f);
    renderDevice->SetBufferData(lightBuffer, &lightInfo, sizeof(LightInfo));

    renderDevice->AttachUniformBuffer(waveBuffer, 1);
    renderDevice->AttachUniformBuffer(lightBuffer, 3);
    renderer->DrawMesh(planeMesh, wavePS);

    // draw the skybox
    renderDevice->BindCubemap(skyboxTexture);
    renderer->DrawMesh(skyboxMesh, skyboxPS);
    
    renderer->End();

    DrawUI();

    renderDevice->EndRenderPass();
    renderDevice->SchedulePresentation();
    renderDevice->SubmitCommandBuffer();
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

      ImGui::ColorEdit4("Water Color", &lightInfo.waveColor[0]);
      ImGui::DragFloat3("Light Pos", &lightInfo.lightPos[0]);

      if (changed)
        GenerateWaves();

      ImGui::PopItemWidth();
    }
    ImGui::End();
    uiRenderer->End();
  }

  void OnResize()
  {
    camera->SetWindowSize(displayWidth, displayHeight);
  }
};

int main()
{
  Waves* waves = new Waves();
  waves->Run();
  delete waves;
}