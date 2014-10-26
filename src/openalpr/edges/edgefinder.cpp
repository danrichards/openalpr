/*
 * Copyright (c) 2014 New Designs Unlimited, LLC
 * Opensource Automated License Plate Recognition [http://www.openalpr.com]
 *
 * This file is part of OpenAlpr.
 *
 * OpenAlpr is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License
 * version 3 as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "edgefinder.h"
#include "textlinecollection.h"

using namespace std;
using namespace cv;

EdgeFinder::EdgeFinder(PipelineData* pipeline_data) {
  
  this->pipeline_data = pipeline_data;
  
  // First re-crop the area from the original picture knowing the text position
  this->confidence = 0;
  
}


EdgeFinder::~EdgeFinder() {
}

std::vector<cv::Point2f> EdgeFinder::findEdgeCorners() {

  TextLineCollection tlc(pipeline_data->textLines);
  
  vector<Point> corners;
  
  // If the character segment is especially small, just expand the existing box
  // If it's a nice, long segment, then guess the correct box based on character height/position
  if (tlc.longerSegment.length > tlc.charHeight * 3)
  {
    float charHeightToPlateWidthRatio = pipeline_data->config->plateWidthMM / pipeline_data->config->charHeightMM;
    float idealPixelWidth = tlc.charHeight *  (charHeightToPlateWidthRatio * 1.03);	// Add 3% so we don't clip any characters

    float charHeightToPlateHeightRatio = pipeline_data->config->plateHeightMM / pipeline_data->config->charHeightMM;
    float idealPixelHeight = tlc.charHeight *  charHeightToPlateHeightRatio;


    float verticalOffset = (idealPixelHeight * 1.5 / 2);
    float horizontalOffset = (idealPixelWidth * 1.25 / 2);
    LineSegment topLine = tlc.centerHorizontalLine.getParallelLine(verticalOffset);
    LineSegment bottomLine = tlc.centerHorizontalLine.getParallelLine(-1 * verticalOffset);

    LineSegment leftLine = tlc.centerVerticalLine.getParallelLine(-1 * horizontalOffset);
    LineSegment rightLine = tlc.centerVerticalLine.getParallelLine(horizontalOffset);

    Point topLeft = topLine.intersection(leftLine);
    Point topRight = topLine.intersection(rightLine);
    Point botRight = bottomLine.intersection(rightLine);
    Point botLeft = bottomLine.intersection(leftLine);

    corners.push_back(topLeft);
    corners.push_back(topRight);
    corners.push_back(botRight);
    corners.push_back(botLeft);
  }
  else
  {

    //cout << "HEYOOO!" << endl;
    int expandX = (int) ((float) pipeline_data->crop_gray.cols) * 0.15f;
    int expandY = (int) ((float) pipeline_data->crop_gray.rows) * 0.15f;
    int w = pipeline_data->crop_gray.cols;
    int h = pipeline_data->crop_gray.rows;
    
    corners.push_back(Point(-1 * expandX, -1 * expandY));
    corners.push_back(Point(expandX + w, -1 * expandY));
    corners.push_back(Point(expandX + w, expandY + h));
    corners.push_back(Point(-1 * expandX, expandY + h));
    
//    for (int i = 0; i < 4; i++)
//    {
//      std::cout << "CORNER: " << corners[i].x << " - " << corners[i].y << std::endl;
//    }
  }


  Transformation imgTransform(pipeline_data->grayImg, pipeline_data->crop_gray, pipeline_data->regionOfInterest);
  vector<Point2f> remappedCorners = imgTransform.transformSmallPointsToBigImage(corners);

//  cout << "topLeft: " << remappedCorners[0] << endl;
//  cout << "topRight: " << remappedCorners[1] << endl;
//  cout << "botRight: " << remappedCorners[2] << endl;
//  cout << "botLeft: " << remappedCorners[3] << endl;
  
  Size cropSize = imgTransform.getCropSize(remappedCorners, 
          Size(pipeline_data->config->templateWidthPx, pipeline_data->config->templateHeightPx));

  Mat transmtx = imgTransform.getTransformationMatrix(remappedCorners, cropSize);
  Mat newCrop = imgTransform.crop(cropSize, transmtx);

//  drawAndWait(&newCrop);
  
  vector<TextLine> newLines;
  for (uint i = 0; i < pipeline_data->textLines.size(); i++)
  {        
    vector<Point2f> textArea = imgTransform.transformSmallPointsToBigImage(pipeline_data->textLines[i].textArea);
    vector<Point2f> linePolygon = imgTransform.transformSmallPointsToBigImage(pipeline_data->textLines[i].linePolygon);

    vector<Point2f> textAreaRemapped;
    vector<Point2f> linePolygonRemapped;

    textAreaRemapped = imgTransform.remapSmallPointstoCrop(textArea, transmtx);
    linePolygonRemapped = imgTransform.remapSmallPointstoCrop(linePolygon, transmtx);

    newLines.push_back(TextLine(textAreaRemapped, linePolygonRemapped));
  }

  PlateLines plateLines(pipeline_data);
  plateLines.processImage(newCrop, newLines, 1.2);


  PlateCorners cornerFinder(newCrop, &plateLines, pipeline_data, newLines);
  vector<Point> smallPlateCorners = cornerFinder.findPlateCorners();

  confidence = cornerFinder.confidence;

//  cout << "topLeft: " << smallPlateCorners[0] << endl;
//  cout << "topRight: " << smallPlateCorners[1] << endl;
//  cout << "botRight: " << smallPlateCorners[2] << endl;
//  cout << "botLeft: " << smallPlateCorners[3] << endl;

//  cvtColor(newCrop, newCrop, CV_GRAY2BGR);
//  for (int i = 0; i < 4; i++)
//    circle(newCrop, smallPlateCorners[i], 4, Scalar(0,255,0), -1);
//  
//  drawAndWait(&newCrop);

//  Transformation reTrans(pipeline_data->crop_gray, newCrop, cv::Rect(0,0,pipeline_data->crop_gray.cols, pipeline_data->crop_gray.rows));
//  vector<Point2f> cornersInOriginalCrop = reTrans.transformSmallPointsToBigImage(smallPlateCorners);
//  
//  for (int i = 0; i < 4; i++)
//    circle(pipeline_data->crop_gray, cornersInOriginalCrop[i], 4, Scalar(255,255,255), -1);
//  
//  drawAndWait(&pipeline_data->crop_gray);

  std::vector<Point2f> imgArea;
  imgArea.push_back(Point2f(0, 0));
  imgArea.push_back(Point2f(newCrop.cols, 0));
  imgArea.push_back(Point2f(newCrop.cols, newCrop.rows));
  imgArea.push_back(Point2f(0, newCrop.rows));
  Mat newCropTransmtx = imgTransform.getTransformationMatrix(imgArea, remappedCorners);

  vector<Point2f> cornersInOriginalImg = imgTransform.remapSmallPointstoCrop(smallPlateCorners, newCropTransmtx);

//  cout << "topLeft: " << cornersInOriginalImg[0] << endl;
//  cout << "topRight: " << cornersInOriginalImg[1] << endl;
//  cout << "botRight: " << cornersInOriginalImg[2] << endl;
//  cout << "botLeft: " << cornersInOriginalImg[3] << endl;

//  Mat debugimg;
//  cvtColor(pipeline_data->crop_gray, debugimg, CV_GRAY2BGR);
//  for (int i = 0; i < 4; i++)
//    circle(debugimg, cornersInOriginalImg[i], 4, Scalar(0,255,0), -1);
//  drawAndWait(&debugimg);


//  for (int i = 0; i < 4; i++)
//    circle(pipeline_data->colorImg, cornersInOriginalImg[i], 4, Scalar(0,255,0), -1);

  return cornersInOriginalImg;

}

