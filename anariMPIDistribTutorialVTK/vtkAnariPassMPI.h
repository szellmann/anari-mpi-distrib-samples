#pragma once

#include "vtkNew.h"
#include "vtkAnariPass.h"

VTK_ABI_NAMESPACE_BEGIN

class vtkCamera;
class vtkRenderWindow;

class vtkAnariPassMPI : public vtkAnariPass
{
public:
  static vtkAnariPassMPI *New();
  vtkTypeMacro(vtkAnariPassMPI, vtkRenderPass);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  void SetRenderWindow(vtkRenderWindow* win);

  virtual void Render(const vtkRenderState* s) override;

  void SetSceneGraph(vtkAnariRendererNode*);

  void StartEventLoop();

protected:
  vtkAnariPassMPI();
  virtual ~vtkAnariPassMPI();

  vtkRenderWindow* RenderWindow{nullptr};

  vtkCamera* GetCamera();

  int *GetSize();
  void SetSize(int w, int h);

private:
  vtkAnariPassMPI(const vtkAnariPassMPI&) = delete;
  void operator=(const vtkAnariPassMPI&) = delete;
};

VTK_ABI_NAMESPACE_END
