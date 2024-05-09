#include <Platform.h> // Only use angled for Platform, else, xcode project won't compile
#include "../cpp_foundation/XBuffer.h"

int XBuffer_tests()
{

#ifdef JIEF_DEBUG
//	printf("XBuffer_tests -> Enter\n");
#endif

  {
    XBuffer<UINT8> xb_uint8;
    void* p = (void*)1;
    char* p2 = (char*)2;
    xb_uint8.cat(p);
    xb_uint8.cat(p2);
    xb_uint8.cat(uintptr_t(0));

    XBuffer<UINT8> xb_uint8_2;
    xb_uint8_2.cat(uintptr_t(1));
    xb_uint8_2.cat(uintptr_t(2));
    xb_uint8_2.cat(uintptr_t(0));

    if ( xb_uint8_2 != xb_uint8 ) return 1;
  }
  {
    XBuffer<UINT8> xb_uint8;
    xb_uint8.S8Catf("%s %d %ls", "s1", 1, L"ls1");
    xb_uint8.S8Catf("%s %d %ls", "s2", 2, L"ls2");
    
    if ( memcmp(xb_uint8.data(), "s1 1 ls1s2 2 ls2", strlen("s1 1 ls1 s2 2 ls2")) != 0 ) return 2;
  }
  

  return 0;
}
