/*
 *   C++ UDP socket server for live image upstreaming
 *   Modified from http://cs.ecs.baylor.edu/~donahoo/practical/CSockets/practical/UDPEchoServer.cpp
 *   Copyright (C) 2015
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <fstream>

#include <sstream>
#include <fstream>

#include "PracticalSocket.h" // For UDPSocket and SocketException
#include <iostream>          // For cout and cerr
#include <cstdlib>           // For atoi()
#include <thread>
#define BUF_LEN 65540 // Larger than maximum UDP packet size

#include "opencv2/opencv.hpp"
using namespace cv;
#include "config.h"

#include <iostream>
#include <chrono>
#include <opencv2/core.hpp>
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <sstream>
#include <string>

#include <chrono>
#include <iostream>
#include <thread>

using namespace std;
using namespace cv;
using std::chrono::system_clock;
using std::chrono::time_point;  
using namespace std::chrono;

cv::VideoCapture inputVideo;
Mat udpFrame;
bool isRun = true;
std::thread t1;
std::thread t2;
Mat resFrame;
Mat camFrame;

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
            if (i == 0)
            {
                double fps = 25.0;   
                std::cout << fps << std::endl;            
                time_point<system_clock> t = system_clock::now();
                fileName = "/media/leolander/Tom F/Videos/udp-image-streaming-save-combine/" + fmt::format("{:%d-%m-%Y %H:%M:%OS}", t) + ".avi";
                video = VideoWriter(fileName.c_str(), cv::VideoWriter::fourcc('M','J','P','G'), fps, Size(frame.cols,frame.rows));
                video.write(frame);
                
                i++;            
            }
            else
            {
                video.write(frame);
                
                cv::putText(frame, "REC",{frame.cols - 60, frame.rows - 20}, cv::FONT_HERSHEY_DUPLEX, 0.7, CV_RGB(200, 0, 0), 2); 
                            
                if (getCurrentMs() - lastFileCheckTimeMs > 10000)
                {
                    lastFileCheckTimeMs = getCurrentMs();
                    isExistRecFile = ( access( fileName.c_str(), F_OK ) != -1 );
                }
                if (isExistRecFile)            
                    cv::putText(frame, "success",{frame.cols - 100, frame.rows - 50}, cv::FONT_HERSHEY_DUPLEX, 0.7, CV_RGB(0, 220, 0), 2); 
                else
                {
                    for (int i = (int)(frame.rows/10); i < frame.rows; i += (int)(frame.rows/10))
                        for(int j = 0; j < frame.cols; j += (int)(frame.cols/10))
                            cv::putText(frame, "ERROR REC!",{j, i}, cv::FONT_HERSHEY_DUPLEX, 0.7, CV_RGB(230, 130, 0), 2); 
                }
                i++;            
            }
            return PrintTime(frame);
        }
        for (int i = (int)(frame.rows/5); i < frame.rows; i += (int)(frame.rows/5))
                        for(int j = 0; j < frame.cols; j += (int)(frame.cols/5))
                            cv::putText(frame, "no recording",{j, i}, cv::FONT_HERSHEY_DUPLEX, 0.7, CV_RGB(119, 221, 231), 2); 
        return frame;    
    }
    
  private:
    string fileName;
    bool isExistRecFile;
    long lastFileCheckTimeMs = 0;
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
      if (H < 10){stream << "0" << H << ":";}else{stream << H << ":";}
      if (M - 60*H < 10){stream << "0" << M - 60*H << ":";}else{stream << M - 60*H << ":";}
      if (S - 60*M - 60*60*H < 10){stream << "0" << S - 60*M - 60*60*H;}else{stream << S - 60*M - 60*60*H;}       
      cv::putText(frame_, stream.str() ,{10, frame_.rows - 10}, cv::FONT_HERSHEY_DUPLEX, 0.7, CV_RGB(255, 255, 255), 2); 
      return frame_;
    };

};

auto now() { return std::chrono::steady_clock::now(); }

auto awake_time()
{
    using std::chrono::operator""ms;
    return now() + 2000ms;
}


void ShowThread()
{
    MyWriter myWriter = MyWriter();
    auto lastWake = now();
    Mat udpFrame2, camFrame2;
    while (isRun)
    {        
        if(!udpFrame.empty() && !camFrame.empty())
        {
            udpFrame.copyTo(udpFrame2);
            camFrame.copyTo(camFrame2);
            if (udpFrame2.rows > camFrame2.rows)
            {
                Mat d_img(udpFrame2.rows - camFrame2.rows, camFrame2.cols, CV_8UC3, Scalar(0,0,0));
                vconcat(camFrame2, d_img, camFrame2); 
            }
            else if (udpFrame2.rows < camFrame2.rows)
            {
                Mat d_img(camFrame2.rows - udpFrame2.rows, udpFrame2.cols, CV_8UC3, Scalar(0,0,0));
                vconcat(udpFrame2, d_img, udpFrame2); 
            }
            std::cout << "udp rows: " << udpFrame2.rows << " cam rows: " << camFrame2.rows;
            hconcat(camFrame2, udpFrame2, resFrame);
            resFrame = myWriter.WriteFrame(resFrame);
            if (resFrame.cols > 1366 || resFrame.rows > 768)
            {
                float dimX = 1366.0/(float)resFrame.cols; 
                float dimY = 768.0/(float)resFrame.rows;
                float dim = dimX < dimY ? dimX : dimY;
                resize(resFrame, resFrame, Size((int)(resFrame.cols * dim), (int)(resFrame.rows * dim)));
            }
            
            imshow("Combine recoder", resFrame);                
            int c = waitKey(1);
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
        std::this_thread::sleep_until(lastWake + 40ms);
        lastWake = now();
    }
    

}

void CamThread()
{
    ifstream work_file("camParams.csv");
    string line;
    getline(work_file, line);
    getline(work_file, line);
    char delimiter = ',';
    stringstream stream(line); // Преобразование строки в поток    
    string camID, height, width, fps, pixelFormat;
    getline(stream, camID, delimiter);
    getline(stream, width, delimiter);
    getline(stream, height, delimiter);
    getline(stream, fps, delimiter);    
    getline(stream, pixelFormat, delimiter);
    work_file.close();    
    inputVideo = cv::VideoCapture(stoi(camID));  

    if (pixelFormat == "MJPG")
    {
        inputVideo.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
        std::cout << "MJPG\n";
    }
    else 
    {
        inputVideo.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V'));
        std::cout << "YUYV\n";
    }   
    inputVideo.set(cv::CAP_PROP_FORMAT, CV_8UC1);
    inputVideo.set(cv::CAP_PROP_FRAME_HEIGHT, stoi(height));
    inputVideo.set(cv::CAP_PROP_FRAME_WIDTH, stoi(width));
    inputVideo.set(cv::CAP_PROP_FPS, stoi(fps));
    
     

    //namedWindow("recv", WINDOW_AUTOSIZE);
        
   
    while (isRun)
    {   
        inputVideo.read(camFrame);
        
        
        


    }
    

}



int main(int argc, char * argv[]) {
    

    if (argc != 2) { // Test for correct number of parameters
        cerr << "Usage: " << argv[0] << " <Server Port>" << endl;
        exit(1);
    }

    unsigned short servPort = atoi(argv[1]); // First arg:  local port



    UDPSocket sock(servPort);

    char buffer[BUF_LEN]; // Buffer for echo string
    int recvMsgSize; // Size of received message
    string sourceAddress; // Address of datagram source
    unsigned short sourcePort; // Port of datagram source

    clock_t last_cycle = clock();
    int c = -1;
   
    
    imshow("Combine recoder", imread("logo.png"));
    waitKey(1);    
    bool isFirst = true;
    while (isRun) {
        // Block until receive message from a client
        do {
            recvMsgSize = sock.recvFrom(buffer, BUF_LEN, sourceAddress, sourcePort);
        } while (recvMsgSize > sizeof(int));
        int total_pack = ((int * ) buffer)[0];

        cout << "expecting length of packs:" << total_pack << endl;
        char * longbuf = new char[PACK_SIZE * total_pack];
        for (int i = 0; i < total_pack; i++) {
            recvMsgSize = sock.recvFrom(buffer, BUF_LEN, sourceAddress, sourcePort);
            if (recvMsgSize != PACK_SIZE) {
                cerr << "Received unexpected size pack:" << recvMsgSize << endl;
                continue;
            }
            memcpy( & longbuf[i * PACK_SIZE], buffer, PACK_SIZE);
        }

        cout << "Received packet from " << sourceAddress << ":" << sourcePort << endl;

        Mat rawData = Mat(1, PACK_SIZE * total_pack, CV_8UC1, longbuf);
        udpFrame = imdecode(rawData, IMREAD_COLOR);
        if (udpFrame.size().width == 0) {
            cerr << "decode failure!" << endl;
            continue;
        }
        
        free(longbuf);
        clock_t next_cycle = clock();
        double duration = (next_cycle - last_cycle) / (double) CLOCKS_PER_SEC;
        cout << "\teffective FPS:" << (1 / duration) << " \tkbps:" << (PACK_SIZE * total_pack / duration / 1024 * 8) << endl;

        cout << next_cycle - last_cycle;
        last_cycle = next_cycle;
        if (isFirst)
        {
            isFirst = false;
            t1 = std::thread(CamThread);
            t2 = std::thread(ShowThread);
                        
        }
    }


    return 0;
}
