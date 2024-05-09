#include <Platform.h> // Only use angled for Platform, else, xcode project won't compile
#include "../cpp_foundation/XStringArray.h"

int XStringArray_tests()
{

#ifdef JIEF_DEBUG
//	printf("XStringWArray_tests -> Enter\n");
#endif

    // Test ConstXStringWArray
    {
        ConstXStringWArray constArray;
        constArray.Add(L"aa");
        XStringW ws = L"bb"_XSW;
        constArray.AddReference(&ws, false);
        const XStringW ws2 = L"cc"_XSW;
        constArray.AddReference(&ws2, false);
        
        XStringWArray array;
        array.Add(L"aa");
        array.AddReference(&ws, false);
        array.Add(L"cc");

        bool b = array == constArray;
        if ( !b ) return 5;
    }

    // test contains
    {
        XStringWArray array1;

        if ( !array1.isEmpty() ) return 1;

        array1.Add(L"word1"_XSW);
        if ( array1.isEmpty() ) return 2;
        if ( array1[0] != "word1"_XS8 ) return 21;
        array1.Add(L"other2"_XSW);
        if ( array1[1] != "other2"_XS8 ) return 21;

        if ( !array1.contains(L"other2"_XSW) ) return 5;
        if ( !array1.containsIC(L"oTHer2"_XSW) ) return 6;
    }
    
  {
    XStringWArray arrayW1;
    arrayW1.Add(L"word1"_XSW);
    arrayW1.Add(L"other2"_XSW);

    XStringWArray arrayW1bis;
    arrayW1bis = arrayW1;
    if ( arrayW1bis != arrayW1 ) return 11;

    {
      XString8Array array81bis;
      array81bis = arrayW1;
      if ( array81bis != arrayW1 ) return 11;
    }
    {
      XString8Array array81bis(arrayW1);
      if ( array81bis != arrayW1 ) return 11;
    }
  }

	// Test == and !=
	{
        
        XStringWArray array1;
        array1.Add(L"word1"_XSW);
        array1.Add(L"other2"_XSW);

		XStringWArray array1bis;
		array1bis.Add(L"word1"_XSW);
		array1bis.Add(L"other2"_XSW);

		if ( !(array1 == array1bis) ) return 10;
		if ( array1 != array1bis ) return 11;
	}
	
	// Split
	{
		XString8Array array = Split<XString8Array>("   word1   word2    word3   ", " ");
		if ( array[0] != "word1"_XS8 ) return 31;
		if ( array[1] != "word2"_XS8 ) return 32;
		if ( array[2] != "word3"_XS8 ) return 33;
	}
	{
		XString8Array array = Split<XString8Array>("word1, word2, word3", ", ");
		if ( array[0] != "word1"_XS8 ) return 41;
		if ( array[1] != "word2"_XS8 ) return 42;
		if ( array[2] != "word3"_XS8 ) return 43;
	}
	{
		XString8Array array = Split<XString8Array>("   word1   word2    word3   "_XS8, " "_XS8);
		if ( array[0] != "word1"_XS8 ) return 51;
		if ( array[1] != "word2"_XS8 ) return 52;
		if ( array[2] != "word3"_XS8 ) return 53;
	}
    {
        XString8Array array = Split<XString8Array>("   word1   word2    word3   "_XS8, " "_XS8);
        XString8 xs = array.ConcatAll(' ', '^', '$');
        if ( xs  != "^word1 word2 word3$"_XS8 ) return 31;
    }
    
    // Test concat and Split
    {
        XStringWArray array;
        array.Add(L"word1"_XSW);
        array.Add("other2"_XS8);
        array.Add("3333");
        array.Add(L"4th_item");
        {
            XStringW c = array.ConcatAll(L", "_XSW, L"^"_XSW, L"$"_XSW);
            if ( c != L"^word1, other2, 3333, 4th_item$"_XSW ) return 1;
        }
        {
            XStringW c = array.ConcatAll(L", ", L"^", L"$");
            if ( c != L"^word1, other2, 3333, 4th_item$"_XSW ) return 1;
        }

        // Split doesn't handle prefix and suffix yet.
        XStringW c = array.ConcatAll(L", ");

        XStringWArray arraybis = Split<XStringWArray>(c);
        if ( array != arraybis ) return 20;
        XString8Array array3bis = Split<XString8Array>(c);
        if ( array != array3bis ) return 20;
    }
    
    // Test Split char[64]
    {
        char buf[64];
        strcpy(buf, "word1 other2 3333 4th_item");
        XString8Array array = Split<XString8Array>(buf, " ");

        if ( array[0] != "word1"_XS8 ) return 31;
        if ( array[1] != "other2"_XS8 ) return 32;
        if ( array[2] != "3333"_XS8 ) return 33;
        if ( array[3] != "4th_item"_XS8 ) return 34;
    }
    
    // Test concat and Split @Pene
    {
        XString8Array array;
        array.Add(L"word1");
        array.Add(L"other2");
        array.Add(L"3333");
        array.Add(L"4th_item");
        
        XString8Array LoadOptions2;
        
        LoadOptions2 = Split<XString8Array>(array.ConcatAll(" "_XS8).c_str(), " ");
        if ( LoadOptions2 != array ) return 22;
        
        LoadOptions2 = Split<XString8Array>(array.ConcatAll(" "_XS8), " ");
        if ( LoadOptions2 != array ) return 22;
        
        LoadOptions2 = Split<XString8Array>(array.ConcatAll(" "_XS8), " "_XS8);
        if ( LoadOptions2 != array ) return 22;
        
        LoadOptions2 = Split<XString8Array>(array.ConcatAll(" "), " ");
        if ( LoadOptions2 != array ) return 22;
        
        LoadOptions2 = array;
        if ( LoadOptions2 != array ) return 22;
    }
    
    // test == versus Same
    {
        XStringWArray array;
        array.Add(L"word1"_XSW);
        array.Add(L"other2"_XSW);
        array.Add(L"3333"_XSW);
        array.Add(L"4th_item"_XSW);

        XStringWArray array2;
        array2.Add(L"word1"_XSW);
        array2.Add(L"3333"_XSW);
        array2.Add(L"other2"_XSW);
        array2.Add(L"4th_item"_XSW);

        if ( array == array2 ) return 40; // Array != because order is different
        if ( !array.Same(array2) ) return 41; // Arrays are the same

    }
    
    // Test AddNoNull and AddID
    {
        XStringWArray array1;
        array1.Add(L"word1"_XSW);
        array1.Add(L"other2"_XSW);

        array1.AddNoNull(L"3333"_XSW);
        if ( array1.size() != 3 ) return 50;
        array1.AddNoNull(L""_XSW);
        if ( array1.size() != 3 ) return 51;
        array1.AddEvenNull(XStringW());
        if ( array1.size() != 4 ) return 52;
        array1.AddID(L"other2"_XSW);
        if ( array1.size() != 4 ) return 53;
    }
    
    // Test operator =
    {
        XString8Array array;
        array.Add(L"word1");
        array.Add(L"other2");
        array.Add(L"3333");
        array.Add(L"4th_item");
		
        XString8Array array2 = array;
        if ( array2 != array ) return 22;
        XString8Array* array2Ptr = &array2;
        *array2Ptr = array2;
        if ( array2 != array ) return 22;
        
        XStringWArray warray1;
        warray1 = array;
        XString8Array array3 = warray1;
        if ( array3 != array ) return 24;
	  }
   
    // test remove() and removeIC()
    {
        XString8Array array;
        array.Add(L"word1");
        array.Add(L"other2");
        array.Add(L"3333");
        array.Add(L"4th_item");
		
        array.remove("WOrd1"_XS8);
        if ( !array.contains("word1"_XS8) ) return 22;
        array.remove("word1"_XS8);
        if ( array.contains("word1"_XS8) ) return 22;
        array.removeIC("oTHEr2"_XS8);
        if ( array.contains("other2"_XS8) ) return 22;
        array.removeIC("4th_ITEM"_XS8);
        if ( array.contains("4th_item"_XS8) ) return 22;
        //XString8 c = array.ConcatAll();
        //printf("c=%s\n", c.c_str());
    }
    
    // test remove until array is empty
    {
        XString8Array array;
        array.Add(L"splash");
        array.Add(L"quiet");

        array.remove("splash"_XS8);
        if ( array.contains("splash﻿"_XS8) ) return 22;
        array.removeIC("quiet"_XS8);
        if ( array.contains("quiet"_XS8) ) return 22;
        if ( array.size() != 0 ) return 22;
        //XString8 c = array.ConcatAll();
        //printf("c=%s\n", c.c_str());
	}



  return 0;
}
