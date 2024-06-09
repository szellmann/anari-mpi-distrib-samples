#pragma once

#include <IceT.h>
#include <IceTMPI.h>

namespace util {

  struct IceTCompositor {
    void destroy() {
      icetDestroyMPICommunicator(icetComm);
    }

    void resize(int width, int height) {
      if (!icetComm) {
        icetComm = icetCreateMPICommunicator(MPI_COMM_WORLD);
        icetCtx = icetCreateContext(icetComm);
      }
      W=width;
      H=height;
      icetSetContext(icetCtx);
      icetResetTiles();
      icetAddTile(0, 0, width, height, 0/*display rank*/);
      icetDisable(ICET_COMPOSITE_ONE_BUFFER); // w/ depth
      icetStrategy(ICET_STRATEGY_REDUCE);
      icetCompositeMode(ICET_COMPOSITE_MODE_Z_BUFFER);
      icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_UBYTE);
      icetSetDepthFormat(ICET_IMAGE_DEPTH_FLOAT);
    }
 
    unsigned char *compositeFrame(const unsigned *fbPointer, const float *dbPointer) {
      icetSetContext(icetCtx);
      IceTFloat bg[4] = { 0.0, 0.0, 0.0, 0.0 };
      int vp[4] = { 0, 0, W, H };
      IceTDouble mv[16], proj[16];
      for (int i=0; i<16; ++i) {
        mv[i]=(i%5==0)?1.:0.;
        proj[i]=(i%5==0)?1.:0.;
      }
      icetImg = icetCompositeImage(fbPointer, dbPointer, vp, proj, mv, bg);
      if (!icetImageIsNull(icetImg)) {
         return icetImageGetColorub(icetImg);
      }
      return nullptr;
    }

   private:
    IceTCommunicator icetComm{0};
    IceTContext icetCtx;
    IceTImage icetImg;
    int W,H;
  };  
} // util
