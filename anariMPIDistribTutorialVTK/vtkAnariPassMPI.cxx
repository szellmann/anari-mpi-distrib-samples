
#include "vtkAnariPassMPI.h"

#include "vtkAnariRendererNode.h"
#include "vtkAnariViewNodeFactory.h"
#include "vtkCamera.h"
#include "vtkObjectFactory.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"

#include <mpi.h>

VTK_ABI_NAMESPACE_BEGIN

enum Command {
  Cmd_Camera,
  Cmd_Render,
};

// ----------------------------------------------------------------------------
vtkStandardNewMacro(vtkAnariPassMPI);

vtkAnariPassMPI::vtkAnariPassMPI()
{
}

vtkAnariPassMPI::~vtkAnariPassMPI()
{
}

void vtkAnariPassMPI::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

void vtkAnariPassMPI::SetRenderWindow(vtkRenderWindow* win)
{
  this->RenderWindow = win;
}

void vtkAnariPassMPI::Render(const vtkRenderState* s)
{
  int mpiRank = 0;
  int mpiWorldSize = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpiWorldSize);

  if (mpiRank == 0) {
    if (auto* cam = GetCamera()) {
      Command cmd = Cmd_Camera;
      MPI_Bcast(&cmd, sizeof(cmd), MPI_BYTE, 0, MPI_COMM_WORLD);

      double *pos = cam->GetPosition();
      double *view = cam->GetFocalPoint();
      double *up = cam->GetViewUp();
      double dist = cam->GetDistance();

      MPI_Bcast(pos, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
      MPI_Bcast(view, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
      MPI_Bcast(up, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
      MPI_Bcast(&dist, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }

  // only the main rank does this!
    Command cmd = Cmd_Render;
    MPI_Bcast(&cmd, sizeof(cmd), MPI_BYTE, 0, MPI_COMM_WORLD);
  }

  vtkAnariPass::Render(s);
}

void vtkAnariPassMPI::StartEventLoop()
{
  auto nextCommand = []() {
    Command cmd;
    MPI_Bcast(&cmd, sizeof(cmd), MPI_BYTE, 0, MPI_COMM_WORLD);
    return cmd;
  };

  for (;;) {
    Command cmd = nextCommand();

    switch (cmd) {
      case Cmd_Camera: {
        double pos[3];
        double view[3];
        double up[3];
        double dist;

        MPI_Bcast(pos, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Bcast(view, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Bcast(up, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Bcast(&dist, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        if (auto* cam = GetCamera()) {
          cam->SetPosition(pos[0], pos[1], pos[2]);
          cam->SetFocalPoint(view[0], view[1], view[2]);
          cam->SetViewUp(up[0], up[1], up[2]);
          cam->SetDistance(dist);
        }

        break;
      }

      case Cmd_Render: {
        this->RenderWindow->Render();
        break;
      }

      default:
        break;
    }
  }
}

vtkCamera* vtkAnariPassMPI::GetCamera()
{
  vtkCamera* cam{nullptr};

  if (this->GetSceneGraph() && this->GetSceneGraph()->GetRenderable())
    cam = vtkRenderer::SafeDownCast(this->GetSceneGraph()->GetRenderable())->GetActiveCamera();

  return cam;
}

VTK_ABI_NAMESPACE_END
