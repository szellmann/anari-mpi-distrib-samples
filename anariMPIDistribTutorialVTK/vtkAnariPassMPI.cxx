
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
  Cmd_Resize,
};

// ----------------------------------------------------------------------------
vtkStandardNewMacro(vtkAnariPassMPI);

vtkAnariPassMPI::vtkAnariPassMPI()
{
  prevSize[0] = -1;
  prevSize[0] = -1;
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

      MPI_Barrier(MPI_COMM_WORLD);
    }

    if (auto *newSize = GetSize()) {
      if (newSize[0] != prevSize[0] || newSize[1] != prevSize[1]) {
        Command cmd = Cmd_Resize;
        MPI_Bcast(&cmd, sizeof(cmd), MPI_BYTE, 0, MPI_COMM_WORLD);
        MPI_Bcast(newSize, 2, MPI_INT, 0, MPI_COMM_WORLD);
        prevSize[0] = newSize[0];
        prevSize[1] = newSize[1];

        MPI_Barrier(MPI_COMM_WORLD);
      }
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

          MPI_Barrier(MPI_COMM_WORLD);
        }

        break;
      }

      case Cmd_Resize: {
        int newSize[2];
        MPI_Bcast(newSize, 2, MPI_INT, 0, MPI_COMM_WORLD);
        this->SetSize(newSize[0], newSize[1]);
        MPI_Barrier(MPI_COMM_WORLD);
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

int *vtkAnariPassMPI::GetSize()
{
  int* size{nullptr};

  if (this->GetSceneGraph())
    size = this->RenderWindow->GetSize();

  return size;
}

void vtkAnariPassMPI::SetSize(int w, int h)
{
  if (this->GetSceneGraph())
    this->RenderWindow->SetSize(w, h);
}

VTK_ABI_NAMESPACE_END
