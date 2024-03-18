#include <mpi.h>

#include "vtkDataSetTriangleFilter.h"
#include "vtkImageCast.h"
#include "vtkNamedColors.h"
#include "vtkNew.h"
#include "vtkOBJReader.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRenderWindow.h"
#include "vtkRenderer.h"
#include "vtkVolume.h"
#include "vtkVolumeProperty.h"
#include "vtkArrayCalculator.h"
#include "vtkWindowToImageFilter.h"
#include "vtkPNGWriter.h"
#include "vtkLogger.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkInteractorStyleTrackballCamera.h"

#include "vtkAnariRendererNode.h"

#include "anari/anari_cpp.hpp"
#include "anari/anari_cpp/ext/linalg.h"

#include "vtkAnariPassMPI.h" // ours!

using namespace anari::math;

inline float3 randomColor(unsigned idx)
{
  unsigned int r = (unsigned int)(idx*13*17 + 0x234235);
  unsigned int g = (unsigned int)(idx*7*3*5 + 0x773477);
  unsigned int b = (unsigned int)(idx*11*19 + 0x223766);
  return float3((r&255)/255.f,
                (g&255)/255.f,
                (b&255)/255.f);
}

struct IdleCallback : vtkCommand
{
  vtkTypeMacro(IdleCallback, vtkCommand);

  static IdleCallback *New() {
    return new IdleCallback;
  }

  void Execute(vtkObject * vtkNotUsed(caller),
               unsigned long vtkNotUsed(eventId),
               void * vtkNotUsed(callData))
  {
    renderWindow->Render();
    static int cnt=0;
    std::cout << cnt++ << '\n';
  }

  vtkRenderWindow *renderWindow;
};

int main(int argc, char *argv[]) {
  int mpiThreadCapability = 0;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &mpiThreadCapability);
  if (mpiThreadCapability != MPI_THREAD_MULTIPLE
      && mpiThreadCapability != MPI_THREAD_SERIALIZED) {
    fprintf(stderr,
        "OSPRay requires the MPI runtime to support thread "
        "multiple or thread serialized.\n");
    return 1;
  }

  int mpiRank = 0;
  int mpiWorldSize = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpiWorldSize);

  vtkLogger::SetStderrVerbosity(vtkLogger::Verbosity::VERBOSITY_WARNING);

  vtkNew<vtkOBJReader> reader;
  reader->SetFileName(argv[1+mpiRank]);
  reader->Update();
  reader->Print(cout);

  // The mapper / ray cast function know how to render the data
  vtkNew<vtkPolyDataMapper> polyMapper;
  polyMapper->SetInputConnection(reader->GetOutputPort());

  auto rc = randomColor(mpiRank);
  vtkColor3d color(rc.x, rc.y, rc.z);

  vtkNew<vtkActor> actor;
  actor->SetMapper(polyMapper);
  actor->GetProperty()->SetDiffuseColor(color.GetData());

  double localBounds[6];
  memcpy(&localBounds[0], actor->GetBounds(), sizeof(double)*6);
  double modelBounds[6];
  MPI_Allreduce(&localBounds[0], &modelBounds[0], 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
  MPI_Allreduce(&localBounds[1], &modelBounds[1], 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  MPI_Allreduce(&localBounds[2], &modelBounds[2], 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
  MPI_Allreduce(&localBounds[3], &modelBounds[3], 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  MPI_Allreduce(&localBounds[4], &modelBounds[4], 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
  MPI_Allreduce(&localBounds[5], &modelBounds[5], 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);

  // Set bounds on the actor to force the renderwindow
  // interactor into performing a viewAll operation:
  for (int i=0; i<6; ++i) {
    actor->GetBounds()[i] = modelBounds[i];
  }

  MPI_Barrier(MPI_COMM_WORLD);

  vtkNew<vtkRenderer> ren1;
  ren1->AddActor(actor);

  ren1->SetBackground(0.3, 0.3, 0.4);
  ren1->ResetCamera();

  // Attach ANARI render pass
  vtkNew<vtkAnariPassMPI> anariPass;
  ren1->SetPass(anariPass);

  vtkAnariRendererNode::SetLibraryName("environment", ren1);
  vtkAnariRendererNode::SetSamplesPerPixel(10, ren1);
  vtkAnariRendererNode::SetLightFalloff(.5, ren1);
  vtkAnariRendererNode::SetUseDenoiser(1, ren1);
  vtkAnariRendererNode::SetCompositeOnGL(1, ren1);

  // Create the renderwindow
  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetMultiSamples(0);
  renderWindow->AddRenderer(ren1);
  renderWindow->SetSize(401, 399); // NPOT size

  // Pass the render window to the render pass,
  // to generate and consume resize events:
  anariPass->SetRenderWindow(renderWindow);

  if (mpiRank == 0) {
    vtkNew<vtkRenderWindowInteractor> iren;
    iren->SetRenderWindow(renderWindow);
    vtkNew<vtkInteractorStyleTrackballCamera> style;
    iren->SetInteractorStyle(style);

    vtkNew<IdleCallback> idleCallback;
    idleCallback->renderWindow = renderWindow;
    iren->CreateRepeatingTimer(1);
    iren->AddObserver(vtkCommand::TimerEvent, idleCallback);

    MPI_Barrier(MPI_COMM_WORLD);

    iren->Start();
  } else {
    renderWindow->SetOffScreenRendering(1);

    MPI_Barrier(MPI_COMM_WORLD);

    anariPass->StartEventLoop();
  }

  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Finalize();

  return 0;
}


