// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__
#ifndef ICP_H
#define ICP_H

#ifdef _MSC_VER
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4996)
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <fstream>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/filesystem/convenience.hpp>
namespace fs = boost::filesystem;

#include <vw/Core.h>
#include <vw/Image.h>
#include <vw/FileIO.h>
#include <vw/Cartography.h>
#include <vw/Math.h>
#include "coregister.h"

using namespace vw;
using namespace vw::math;
using namespace vw::cartography;
using namespace std;


Vector2 back_2_fore_lonlat(Vector2 back_lon_lat);
Vector2 fore_2_back_lonlat(Vector2 fore_lon_lat);

//computes the translation between the foreground and background pixels
void ComputeDEMTranslation(vector<Vector3> featureArray, vector<Vector3> matchArray, Vector3 &translation );

void PrintMatrix(Matrix<float, 3, 3> &A);

//compute the matching error vector and overall average error 
float ComputeMatchingError(vector<Vector3> featureArray, vector<Vector3> matchArray, vector<float> &errorArray);

void ComputeDEMRotation(vector<Vector3> featureArray, vector<Vector3> matchArray, 
                        Vector3 translation, Matrix<float, 3, 3> &rotation);

//applies a 3D rotation and transform to a DEM
void  TransformFeatures(vector<Vector3> &featureArray, Vector3 translation, Matrix<float,3,3> rotation);

void ICP(vector<Vector3> featureArray, vector<Vector3> modelArray, CoregistrationParams settings,
	    Vector3 &translation, Matrix<float, 3, 3> &rotation, vector<float> &errorArray);

//compute a set of features from the fore image.
template <class ViewT>
vector<Vector3> 
GetFeatures(ImageViewBase<ViewT> const& foreImg, GeoReference const &foreGeo,
            ImageViewBase<ViewT> const& backImg, GeoReference const &backGeo, 
            Vector2 samplingStep, Vector2 delta_lonlat)
{
 
  int verStep = samplingStep(0);
  int horStep = samplingStep(1);
 vector<Vector3> featureArray;
 //determine if this is ppd or mpp.
 //determine the modata value as well
 ImageViewRef<typename ViewT::pixel_type>  interpBackImg = interpolate(edge_extend(backImg.impl(),
                                                                           ConstantEdgeExtension()),
                                                                           BilinearInterpolation());
  
 for (int j = 0; j < foreImg.impl().rows(); j=j+verStep){
     for (int i = 0; i < foreImg.impl().cols(); i=i+horStep){
       //determine the location of the corresponding background point

       if ((isnan(foreImg.impl()(i,j)[0])!=FP_NAN) && (foreImg.impl()(i,j)[0]) != 0)  { 

	 Vector2 forePix(i,j);
	 Vector2 fore_lonlat = foreGeo.pixel_to_lonlat(forePix);
         fore_lonlat(0)=fore_lonlat(0)+delta_lonlat(0);
	 fore_lonlat(1)=fore_lonlat(1)+delta_lonlat(1);

         Vector3 fore_lonlat3(fore_lonlat(0), fore_lonlat(1), (foreImg.impl())(i,j));
         Vector3 fore_xyz = foreGeo.datum().geodetic_to_cartesian(fore_lonlat3);
      
         featureArray.push_back(fore_xyz);
       }
     }
  }

 return featureArray;

};

//find the closest background points to the fore points.
template <class ViewT>
void
FindMatches(vector<Vector3> featureArray, ImageViewBase<ViewT> const& backImg, GeoReference const &backGeo, 
            GeoReference const &foreGeo, vector<Vector3>& matchArray, Vector2 matchWindowHalfSize)
{
 
 int matchWindowHalfWidth = matchWindowHalfSize(0);
 int matchWindowHalfHeight = matchWindowHalfSize(1);

 ImageViewRef<typename ViewT::pixel_type>  interpBackImg = interpolate(edge_extend(backImg.impl(),
                                                                           ConstantEdgeExtension()),
                                                                           BilinearInterpolation());
 
 for (int i = 0; i < featureArray.size(); i++){

     //revert fore from cartesian to spherical coordinates
     Vector3 fore_lonlat3 = foreGeo.datum().cartesian_to_geodetic(featureArray[i]);
     Vector2 fore_lonlat;
 
     fore_lonlat(0) = fore_lonlat3(0);
     fore_lonlat(1) = fore_lonlat3(1);

     Vector2 back_lonlat = fore_2_back_lonlat(fore_lonlat);

     Vector2 backPix = backGeo.lonlat_to_pixel(back_lonlat);	 
     float x = backPix(0);
     float y = backPix(1);

     Vector3 back_xyz;
    
     float minDistance = 10000000000.0;
     for (int k =  y-matchWindowHalfHeight; k < y + matchWindowHalfHeight+1; k++){
        for (int l = x - matchWindowHalfWidth; l < x + matchWindowHalfWidth+1; l++){
 
          Vector2 backPix(l,k);
	  Vector2 back_lonlat = backGeo.pixel_to_lonlat(backPix);
           
          //revert to fore lon lat system
          Vector2 t_back_lonlat2 = back_2_fore_lonlat(back_lonlat);
          Vector3 t_back_lonlat3;
          t_back_lonlat3(0) = t_back_lonlat2(0);
          t_back_lonlat3(1) = t_back_lonlat2(1); 
          t_back_lonlat3(2) = interpBackImg(l,k);
      
          //transform into xyz coordinates of the foregound image.
          Vector3 t_back_xyz= foreGeo.datum().geodetic_to_cartesian(t_back_lonlat3);             
      
          float distance1 = t_back_xyz(0) - featureArray[i](0);
          float distance2 = t_back_xyz(1) - featureArray[i](1);
          float distance3 = t_back_xyz(2) - featureArray[i](2);
          float distance = sqrt(distance1*distance1 + distance2*distance2 + distance3*distance3);   
          if (distance < minDistance){
	    minDistance = distance;
            back_xyz = t_back_xyz;
          }
  
	}
     }
     
    
     matchArray[i]=back_xyz;
 }
 
};

//determines best DEM points that matche to LOLA track points
//used in ICP
template <class ViewT>
void 
FindMatchesFromDEM(vector<Vector3> lidar_xyz,  ImageViewBase<ViewT> const& DEM, GeoReference const &DEMGeo, 
                   vector<Vector3>& matchArray, Vector3 &translation, Matrix<float, 3, 3> &rotation, 
                   double noDEMVal, Vector2 matchWindowHalfSize)
{
  float radius = DEMGeo.datum().semi_major_axis();
  int matchWindowHalfWidth = matchWindowHalfSize(0);
  int matchWindowHalfHeight = matchWindowHalfSize(1);

  ImageViewRef<float> interpDEM;
  if ( IsMasked<typename ViewT::pixel_type>::value == 0 ) {
    interpDEM = pixel_cast<float>(interpolate(edge_extend(DEM.impl(),
							  ConstantEdgeExtension()),
					      BilinearInterpolation()) );

    //cout << "NOT masked" <<endl;
  } else {
    interpDEM = pixel_cast<float>(interpolate(edge_extend(apply_mask(DEM.impl()),
							  ConstantEdgeExtension()),
					      BilinearInterpolation()) );
    //cout << "MASKED" <<endl;
  }

 
  Vector3 matchCenter;
  for (int i = 0; i < matchArray.size(); i++){  
      matchCenter = matchCenter + matchArray[i];
  }
  matchCenter = matchCenter/matchArray.size();

  for (int index = 0; index < lidar_xyz.size(); index++){
  
    Vector3 lidar_lonlatrad = DEMGeo.datum().cartesian_to_geodetic(lidar_xyz[index]);
    //cout<<"lidar_alt="<<lidar_lonlatrad(2)<<endl;      
    Vector2 lidar_lonlat(lidar_lonlatrad(0), lidar_lonlatrad(1));
    Vector2 DEM_pix = DEMGeo.lonlat_to_pixel(lidar_lonlat);
	  
    float x = DEM_pix[0];
    float y = DEM_pix[1];
    
    float minDistance = 1000000.0;
    //search in a neigborhood around x,y and determine the best match to LOLA
    for (int k =  y-matchWindowHalfHeight; k < y + matchWindowHalfHeight+1; k++){
      for (int l = x - matchWindowHalfWidth; l < x + matchWindowHalfWidth+1; l++){
	if ((l>=0) && (k>=0) && (l<interpDEM.cols()) && (k<interpDEM.rows())){
	  if (interpDEM(l,k)!=noDEMVal){

	    //compute the distance to the lidar point
	    Vector2 pix(l,k);
	    Vector2 lonlat = DEMGeo.pixel_to_lonlat(pix);
                    
	    //revert to lon lat rad system
	    Vector3 lonlatrad;
	    lonlatrad(0) = lonlat(0);
	    lonlatrad(1) = lonlat(1); 
	    lonlatrad(2) = 0.001*(radius + interpDEM(l,k)); //km

	    //transform into xyz coordinates of the foregound image.
	    Vector3 dem_xyz = DEMGeo.datum().geodetic_to_cartesian(lonlatrad);             
            dem_xyz = rotation*(dem_xyz-matchCenter) + matchCenter + translation; 

	    float distance1 = dem_xyz(0) - lidar_xyz[index](0);
	    float distance2 = dem_xyz(1) - lidar_xyz[index](1);
	    float distance3 = dem_xyz(2) - lidar_xyz[index](2);
	    float distance = sqrt(distance1*distance1 + distance2*distance2 + distance3*distance3);   
		           
            if (distance < minDistance){
	       minDistance = distance;
	       matchArray[index] = dem_xyz;
	    }
		    
	  }
	}
      }
    }   
  }
}

template <class ViewT>
void 
RunICP(vector<Vector3> featureArray, ImageViewBase<ViewT> const& backDEM,  
       GeoReference const& backDEMGeo, GeoReference const& foreDEMGeo, CoregistrationParams settings,
       Vector3 &translation, Matrix<float, 3, 3> &rotation, vector<float> &errorArray)
{
   

    vector<Vector3> translationArray;
    vector<Matrix<float, 3,3> > rotationArray;
    vector<Vector3> matchArray;
    matchArray.resize(featureArray.size());
    
    int numIter = 0;
    float matchError = 100.0; 
    rotationArray.clear();
    translationArray.clear();
    
    while((numIter < settings.maxNumIter)&&(matchError > settings.minConvThresh)){
      
	    printf("feature matching ...\n");
     
	    FindMatches(featureArray, backDEM, backDEMGeo, foreDEMGeo, matchArray, settings.matchWindowHalfSize);
      
	    cout<<"computing the matching error ..."<<endl;
	    matchError = ComputeMatchingError(featureArray, matchArray, errorArray);
	    cout<<"match error="<<matchError<<endl;
	  
	    cout<<"computing DEM translation ..."<<endl;
	    ComputeDEMTranslation(featureArray, matchArray, translation);
	    //cout<<"T[0]="<<translation[0]<<" T[1]="<<translation[1]<<" T[2]="<<translation[2]<<endl;
             
	    cout<<"computing DEM rotation ..."<<endl;
	    ComputeDEMRotation(featureArray, matchArray, translation, rotation);
	    //PrintMatrix(rotation);

	    //apply the computed rotation and translation to the featureArray  
	    TransformFeatures(featureArray, translation, rotation);

	    translationArray.push_back(translation);
	    rotationArray.push_back(rotation);
	    
	    numIter++;
            cout<<"numIter="<<numIter<<endl;

    }
    
    rotation = rotationArray[0];
    translation = translationArray[0];
    for (int i = 1; i < rotationArray.size(); i++){
	 rotation = rotation*rotationArray[i];
	 translation = translation + translationArray[i];
    }
 
}



template <class ViewT>
void 
ICP(vector<Vector3> featureArray,  ImageViewBase<ViewT> const& DEM,  
    GeoReference const& DEMGeo, vector<Vector3> modelArray, CoregistrationParams settings,
    Vector3 &translation, Matrix<float, 3, 3> &rotation, vector<float> &errorArray)
{
   

    vector<Vector3> translationArray;
    vector<Matrix<float, 3,3> > rotationArray;
    
    int maxNumIter = 10;
    int numIter = 0;
    float matchError = 100.0; 
    rotationArray.clear();
    translationArray.clear();
    errorArray.resize(featureArray.size());

    
    while((numIter < settings.maxNumIter) && (matchError > settings.minConvThresh)){
      	
            FindMatchesFromDEM(modelArray, DEM, DEMGeo, featureArray, translation, rotation, settings.noDataVal, settings.matchWindowHalfSize);

	    cout<<"computing the matching error ..."<<endl;
	    matchError = ComputeMatchingError(featureArray, modelArray, errorArray);
	    cout<<"match error="<<matchError<<endl;
	  
	    cout<<"computing DEM translation ..."<<endl;
	    ComputeDEMTranslation(featureArray, modelArray, translation);
	    //cout<<"T[0]="<<translation[0]<<" T[1]="<<translation[1]<<" T[2]="<<translation[2]<<endl;
             
	    cout<<"computing DEM rotation ..."<<endl;
	    ComputeDEMRotation(featureArray, modelArray, translation, rotation);
	    //PrintMatrix(rotation);

	    //apply the computed rotation and translation to the featureArray  
	    TransformFeatures(featureArray, translation, rotation);

	    translationArray.push_back(translation);
	    rotationArray.push_back(rotation);
	    
	    numIter++;
            cout<<"numIter="<<numIter<<endl;

    }
    
    rotation = rotationArray[0];
    translation = translationArray[0];
    for (int i = 1; i < rotationArray.size(); i++){
	 rotation = rotation*rotationArray[i];
	 translation = translation + translationArray[i];
    }
 
}

#endif
