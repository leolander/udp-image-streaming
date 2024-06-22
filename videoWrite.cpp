#include "opencv2/opencv.hpp"
#include <iostream>
#include <chrono>
#include <opencv2/core.hpp>
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <sstream>
#include <string>

using namespace std;
using namespace cv;
using std::chrono::system_clock;
using std::chrono::time_point;  
using namespace std::chrono;

long getCurrentMs()
{
    auto now = std::chrono::system_clock::now();        
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = now_ms.time_since_epoch();
    return value.count();
}

class MyWriter
{
  #define delayFramesCount 60
  public:

    void BeginWrite()
    {
      if (!isRec)
      {
        std::cout << "rec" << std::endl;
        isRec = true;
        startTimeMs = getCurrentMs();      
      }
    }

    void EndWrite()
    {
      if (isRec)
      {
        std::cout << "save" << std::endl;
        isRec = false;
        i = 0;
        video.release();
      }
    }

    void Finish()
    {
      isRec = false;
      video.release();
    }

    Mat WriteFrame(Mat frame)
    {
      if (isRec)
      {
        if (i > delayFramesCount)
        {          
          frame = PrintTime(frame);          
          video.write(frame);
          cv::putText(frame, "REC",{frame.cols - 60, frame.rows - 20}, cv::FONT_HERSHEY_DUPLEX, 0.7, CV_RGB(200, 0, 0), 2); 
          i++;
          return frame;
        }
        else if (i < delayFramesCount)
        {
          frame = PrintTime(frame);

          frame.copyTo(records[i]);
          cv::putText(frame, "REC+",{frame.cols - 60, frame.rows - 20}, cv::FONT_HERSHEY_DUPLEX, 0.7, CV_RGB(200, 0, 0), 2);          
          i++;
          return frame;
        }
        else if (i == delayFramesCount)
        {
          double fps = (double)delayFramesCount/(0.001*(double)(getCurrentMs() - startTimeMs));      
          std::cout << fps << std::endl;            
          time_point<system_clock> t = system_clock::now();
          video = VideoWriter(fmt::format("{:%d-%m-%Y %H:%M:%OS}", t) + ".avi", cv::VideoWriter::fourcc('M','J','P','G'), fps, Size(records[0].cols,records[0].rows));
          for(int j = 0; j < i; j++)
            video.write(records[j]);
          i++;            
        }
      }
      return frame;
    }
    
  private:
    VideoWriter video;    
    Mat records[delayFramesCount];
    int i = 0;
    int c = -1;
    bool isRec = false;    
    long startTimeMs;

    Mat PrintTime(Mat frame_)
    {
      milliseconds ms(getCurrentMs() - startTimeMs);
      ostringstream stream;
      int H = duration_cast<hours>(ms).count();
      int M = duration_cast<minutes>(ms).count();
      int S = duration_cast<seconds>(ms).count();
      if (H < 10){stream << "0" << H << ":";}else{stream << H;}
      if (M - 60*H < 10){stream << "0" << M - 60*H << ":";}else{stream << M - 60*H;}
      if (S - 60*M - 60*60*H < 10){stream << "0" << S - 60*M - 60*60*H;}else{stream << S - 60*M - 60*60*H;}       
      cv::putText(frame_, stream.str() ,{10, frame_.rows - 10}, cv::FONT_HERSHEY_DUPLEX, 0.7, CV_RGB(255, 255, 255), 2); 
      return frame_;
    };

};



int main(){
  
  VideoCapture cap(0); 
  if(!cap.isOpened()){
   	cout << "Error opening video stream" << endl;
        return -1;
  }     
  
  Mat frame;  
  cap >> frame;  
  int c = -1;  
  bool isRun = true;  
  MyWriter myWriter = MyWriter();
  while(isRun){
  
    cap >> frame;
    if (frame.empty())
        break;
    frame = myWriter.WriteFrame(frame);
    imshow( "Frame", frame );    
    c = waitKey(1);
    switch (c)
    {
    case 27:
      myWriter.Finish();
      isRun = false;
      break;
    case (int)'r':
    case (int)'R':          
      myWriter.BeginWrite();
      break;
    case (int)'s':
    case (int)'S':
      myWriter.EndWrite();
      break;
    }
  }
  cap.release();  
  destroyAllWindows();
  return 0;
}
