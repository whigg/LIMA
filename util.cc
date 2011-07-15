// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <boost/algorithm/string/join.hpp>

#include <vw/Core.h>
#include <vw/Image.h>
#include <vw/FileIO.h>
#include <vw/Cartography.h>
#include <vw/Math.h>
#include <asp/IsisIO.h>
#include <asp/IsisIO/IsisCameraModel.h>

#include "util.h"

using namespace vw;
using namespace vw::math;
using namespace vw::cartography;
using namespace vw::camera;
using namespace std;


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

void printOverlapList(std::vector<int>  overlapIndices)
{
  printf("numOverlapping images = %d\n", (int)(overlapIndices.size()));
    for (unsigned int i = 0; i < overlapIndices.size(); i++){
      printf("%d ", overlapIndices[i]);
    }
}

//this will be used to compute the makeOverlapList in a more general way.
//it takes into consideration any set of overlapping images.
Vector4 ComputeGeoBoundary(string cubFilename)
{
  GeoReference moonref( Datum("D_MOON"), identity_matrix<3>() );
  boost::shared_ptr<IsisCameraModel> isiscam( new IsisCameraModel( cubFilename ) );
  BBox2 camera_boundary =
    camera_bbox( moonref, boost::shared_dynamic_cast<CameraModel>(isiscam),
		 isiscam->samples(), isiscam->lines() );
 
  Vector4 corners;
 

  float minLon = camera_boundary.min()[0];
  float minLat = camera_boundary.min()[1];
  float maxLon = camera_boundary.max()[0];
  float maxLat = camera_boundary.max()[1];

  //printf("minLon = %f, minLat = %f, maxLon = %f, maxLat = %f\n", minLon, minLat, maxLon, maxLat);
  if (maxLat<minLat){
      float temp = minLat;
      minLat = maxLat;
      maxLat = temp;    
  }

  if (maxLon<minLon){
     float temp = minLon;
     minLon = maxLon;
     maxLon = temp;    
  }

  corners(0) = minLon;
  corners(1) = maxLon;
  corners(2) = minLat;
  corners(3) = maxLat;

  return corners;
}
//this function determines the image overlap for the general case
//it takes into consideration any set of overlapping images.
std::vector<int> makeOverlapList(std::vector<std::string> inputFiles, Vector4 currCorners) {
  
  std::vector<int> overlapIndices;
 
  for (unsigned int i = 0; i < inputFiles.size(); i++){

       int lonOverlap = 0;
       int latOverlap = 0; 
     
       Vector4 corners = ComputeGeoBoundary(inputFiles[i]);
       
       printf("lidar corners = %f %f %f %f\n", currCorners[0], currCorners[1], currCorners[2], currCorners[3]); 
       printf("image corners = %f %f %f %f\n", corners[0], corners[1], corners[2], corners[3]);       

       if(  ((corners(0)>currCorners(0)) && (corners(0)<currCorners(1))) //minlon corners in interval of currCorners 
	  ||((corners(1)>currCorners(0)) && (corners(1)<currCorners(1))) //maxlon corners in interval of currCorners
          ||((currCorners(0)>corners(0)) && (currCorners(0)<corners(1)))
          ||((currCorners(1)>corners(0)) && (currCorners(1)<corners(1)))) 
            
       {
         lonOverlap = 1;
       }
       if(  ((corners(2)>currCorners(2)) && (corners(2)<currCorners(3))) //minlat corners in interval of currCorners
	  ||((corners(3)>currCorners(2)) && (corners(3)<currCorners(3))) //maxlat corners in interval of currCorners
          ||((currCorners(2)>corners(2)) && (currCorners(2)<corners(3))) //minlat corners in interval of currCorners
	  ||((currCorners(3)>corners(2)) && (currCorners(3)<corners(3)))) //maxlat corners in interval of currCorners
	
       {
         latOverlap = 1;
       }
    
       cout<<"lonOverlap="<<lonOverlap<<", latOverlap="<<latOverlap<<endl; 
       cout<<"-----------------------------------------"<<endl;

       if ((lonOverlap == 1) && (latOverlap == 1)){
           overlapIndices.push_back(i);
       }
  }

  //cout<<overlapIndices<<endl;
  return overlapIndices;
}

void
writeErrors(	const string&          filename, 
				const vector<Vector3>& locations, 
				const valarray<float>& errors,
				const vector<string>&  titles,
				const string&          separator,
                const string&          commentor)
{
ofstream file( filename.c_str() );

if( !file ) {
  vw_throw( ArgumentErr() << "Can't open error output file \"" << filename << "\"" );
  }

if( locations.size() != errors.size() ) {
  vw_throw( ArgumentErr() 
  << "The there are a different number of locations (" 
  << locations.size() << ") than errors (" 
  << errors.size() <<") which is a problem." );
  }

if( titles.size() > 0 ){
  file << commentor << " ";
  string title = boost::join( titles, separator );
  file << title << endl;
}

for( unsigned int i = 0; i < locations.size(); i++ ){
  file  << locations[i].x() << separator
		<< locations[i].y() << separator
		<< fixed
		<< locations[i].z() << separator
		<< errors[i] << endl;
  }
}

void MakeTestTable() 
{
  ofstream out("matches_test.dat", ios::out | ios::binary);
  float *scoreArray = new float[10];
  for (int i = 0; i < 10;i++){
    scoreArray[i] = (float)i*1.0;
  }
  //out.seekp(i, ios::beg);
  //out.write((char*)(&scoreArray[0]), 10*sizeof(float));
  
  for (int i = 0; i < 10;i++){
    float score = (float)i;
    cout<<"score="<<score<<endl;
    out.seekp(i*sizeof(float), ios::beg);
    out.write(reinterpret_cast<char*>(&score), sizeof(float));
  }
  
  out.close();
}

void ReadTestTable()
{
  cout<<"reading"<<endl;
  ifstream fp;
  fp.open ("matches_test.dat", ios::in | ios::binary );
  if (!fp.is_open()){
    cout<<"muja"<<endl;
  }
  /*
  //vector<float> scoreArray;
  //scoreArray.resize(10);
  float *scoreArray = new float[10];
  fp.read((char*)&(scoreArray[0]), 10*sizeof(float));
  for (int i = 0; i < 10; i++){
    cout<<i<<":"<<scoreArray[i]<<endl;
  }
  */
  fp.seekg (0, ios::end);
  long length = fp.tellg();
  cout<<"length="<<length<<endl;
  float value;
  for (int i = 0; i < 10; i++){
    fp.seekg(i*sizeof(float), ios::beg);
    cout << "File pointer is at " << fp.tellg() << endl;
    fp.read((char*)&value, sizeof(float));
    cout<<"index: "<<i<<", value: "<<value<<endl;
  }
  fp.close();
}
