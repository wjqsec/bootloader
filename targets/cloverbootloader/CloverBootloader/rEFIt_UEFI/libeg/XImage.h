/*
This class will replace EG_IMAGE structure and methods
*/

#if !defined(__XIMAGE_H__)
#define __XIMAGE_H__

extern "C" {
#include <Protocol/GraphicsOutput.h>
}
#include "../cpp_foundation/XArray.h"
#include "../cpp_foundation/XString.h"
#include "../libeg/libeg.h"
//#include "lodepng.h"
//
//#include "nanosvg.h"
//#include "FloatLib.h"


#if 0 //ndef EFI_GRAPHICS_OUTPUT_BLT_PIXEL
typedef struct {
  UINT8 Blue;
  UINT8 Green;
  UINT8 Red;
  UINT8 Reserved;  //this is Alpha. 0 means full transparent, 0xFF means opaque
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;


typedef union {
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel;
  UINT32                        Raw;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION;
#endif
/*
typedef struct {
  UINTN Xpos;
  UINTN Ypos;
  UINTN Width;
  UINTN Height;
} EgRect;
*/
class XImage
{
protected:
  size_t      Width; //may be better to use INTN - signed integer as it always compared with expressions
  size_t      Height;
  XArray<EFI_GRAPHICS_OUTPUT_BLT_PIXEL> PixelData;
 
public:
  XImage() : Width(0), Height(0), PixelData() {};
  XImage(UINTN W, UINTN H);
  XImage(const XImage& Image, float scale = 0.f); //the constructor can accept 0 scale as 1.f
  virtual ~XImage();

  XImage& operator= (const XImage& other);

  UINTN GetSizeInBytes() const;  //in bytes
  void setSizeInPixels(UINTN W, UINTN H);

  const XArray<EFI_GRAPHICS_OUTPUT_BLT_PIXEL>& GetData() const;

  const EFI_GRAPHICS_OUTPUT_BLT_PIXEL& GetPixel(INTN x, INTN y) const;
  const EFI_GRAPHICS_OUTPUT_BLT_PIXEL* GetPixelPtr(INTN x, INTN y) const ;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL* GetPixelPtr(INTN x, INTN y);
  INTN      GetWidth() const { return (INTN)Width; }
  INTN      GetHeight() const { return (INTN)Height; }

  void setZero() { SetMem( (void*)GetPixelPtr(0, 0), GetSizeInBytes(), 0); }

  void setEmpty() { Width=0; Height=0; PixelData.setEmpty(); }
  XBool isEmpty() const { return PixelData.size() == 0; }


  void Fill(const EFI_GRAPHICS_OUTPUT_BLT_PIXEL& Color = { 0, 0, 0, 0 });
  void Fill(const EG_PIXEL* Color);
  void FillArea(const EG_PIXEL* Color, EG_RECT& Rect);
  void FillArea(const EFI_GRAPHICS_OUTPUT_BLT_PIXEL& Color, EG_RECT& Rect);
  UINT8 Smooth(const UINT8* p, int a01, int a10, int a21, int a12,  float dx, float dy, float scale);
  void Copy(XImage* Image);
  void CopyScaled(const XImage& Image, float scale = 0.f);
  void CopyRect(const XImage& Image, INTN X, INTN Y);
  void CopyRect(const XImage& Image, const EG_RECT& OwnPlace, const EG_RECT& InputRect);
  void Compose(const EG_RECT& OwnPlace, const EG_RECT& InputRect, const XImage& TopImage, XBool Lowest, float TopScale = 0.f);
  void Compose(INTN PosX, INTN PosY, const XImage& TopImage, XBool Lowest, float topScale = 0); //instead of compose we often can Back.Draw(...) + Top.Draw(...)
  void FlipRB();
  EFI_STATUS FromPNG(const UINT8 * Data, UINTN Lenght);
  EFI_STATUS ToPNG(UINT8** Data, UINTN& OutSize);
  EFI_STATUS FromSVG(const CHAR8 *SVGData, float scale);
  EFI_STATUS FromICNS(IN UINT8 *FileData, IN UINTN FileDataLength, IN UINTN IconSize);

  void GetArea(const EG_RECT& Rect);
  void GetArea(INTN x, INTN y, UINTN W, UINTN H);
  void Draw(INTN x, INTN y, float scale, XBool Opaque) const;
  void Draw(INTN x, INTN y, float scale) const; //can accept 0 scale as 1.f
  void Draw(INTN x, INTN y) const;
  void DrawWithoutCompose(INTN x, INTN y, UINTN width = 0, UINTN height = 0) const;
  void DrawOnBack(INTN x, INTN y, const XImage& Plate) const;
//I changed the name because LoadImage is too widely used
// will be used instead of old egLoadImage
  EFI_STATUS LoadXImage(const EFI_FILE *Dir, const XString8& FileName); //for example LoadImage(ThemeDir, L"icons\\" + Name);
  EFI_STATUS LoadXImage(const EFI_FILE *Dir, const wchar_t* LIconName);
  EFI_STATUS LoadXImage(const EFI_FILE *Dir, const char* IconName);
  EFI_STATUS LoadIcns(const EFI_FILE *Dir, IN CONST CHAR16 *FileName, IN UINTN PixelSize);
  void EnsureImageSize(IN UINTN Width, IN UINTN Height, IN CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL& Color);
  void EnsureImageSize(IN UINTN NewWidth, IN UINTN NewHeight);
  void DummyImage(IN UINTN PixelSize);
protected:

};

extern XImage NullXImage;

class IndexedImage
{
public:
protected:
  INTN Id;

  XImage Image;
public:
  INTN getIndex() const { return  Id; }
  void setIndex(INTN Index) { Id = Index; }
  const XImage& getImage() const { return Image; }
  void setImage(const XImage& Sample) { Image = Sample; }

  IndexedImage() : Id(0), Image() {}
  IndexedImage(INTN ID) : Id(ID), Image() {}
  ~IndexedImage() {}
};

#endif //__XSTRINGW_H__
