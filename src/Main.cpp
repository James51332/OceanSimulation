#include "Waves.h"

int main()
{
  Waves::WaveApp *app = new Waves::WaveApp();
  app->Run();
  delete app;
}