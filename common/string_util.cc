// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "string_util.h"

using namespace std;
vector<std::string> FindAndSplit(std::string& tInput, std::string tFind)
{
  vector<std::string> elems;
  size_t uFindLen = tFind.length(); 
  
  if( uFindLen != 0 ){

    int index = tInput.find(tFind);

    while (index != -1){
      string elem(tInput, 0, index);
      tInput.erase(0, index+uFindLen);
      elems.push_back(elem);
      index = tInput.find(tFind); 
    }
  }

  return elems;
}

void FindAndReplace( std::string& tInput, std::string tFind, std::string tReplace ) 
{ 

  size_t uPos = 0; 
  size_t uFindLen = tFind.length(); 
  size_t uReplaceLen = tReplace.length();
  
  if( uFindLen != 0 ){
    for( ;(uPos = tInput.find( tFind, uPos )) != std::string::npos; ){
      tInput.replace( uPos, uFindLen, tReplace );
      uPos += uReplaceLen;
    }
  }

}

///Returns the file extension
std::string GetFilenameExt(std::string const& filename) {
  std::string result = filename;
  int index = result.rfind(".");
  if (index != -1)
    result.erase(0, index+1);
  return result;
}

/// Erases a file suffix if one exists and returns the base string
std::string GetFilenameNoExt(std::string const& filename) {
  std::string result = filename;
  int index = result.rfind(".");
  if (index != -1)
      result.erase(index, result.size());
  return result;
}

// Erases a file path if one exists and returns the base string 
std::string GetFilenameNoPath(std::string const& filename) {
  std::string result = filename;
  int index = result.rfind("/");
  if (index != -1)
    result.erase(0, index+1);
  return result;
}




