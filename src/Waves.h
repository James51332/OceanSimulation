#pragma once

#include "core/App.h"

#include "Generator.h"
#include "Renderer.h"

namespace Waves
{

class WaveApp : public Vision::App
{
public:
  WaveApp();
  ~WaveApp();

  void OnUpdate(float timestep);
  void OnResize(float width, float height);

  void DrawUI();

private:
  Generator *generator = nullptr;
  WaveRenderer *waveRenderer = nullptr;

  Vision::ID renderPass = 0;

  float wavelength = 6.5f;
  float amplitude = 0.10f;
  float angularFrequency = 1.5f;
  float freqDamp = 1.234f;
  float lengthDamp = 0.85f;
  float ampDamp = 0.75f;
};

} // namespace Waves