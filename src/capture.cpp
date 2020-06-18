#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
//#include <opencv2/videoio/legacy/constants_c.h>

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <time.h>

#include <netinet/in.h>
#include <arpa/inet.h>


using namespace cv;
using namespace std;

//#define NETWORK_CLIENT
#if defined(NETWORK_CLIENT)
#define PORT	5032
#endif


int main(int argc, char *argv[])
{
//	std::cout << cv::getBuildInformation() << std::endl;
	const char* devname = "/dev/video2";
	if(argc > 1){
		devname = argv[1];	
	}
		
//	cv::VideoCapture cap("/dev/video2");
	cv::VideoCapture cap(devname);
	cv::Mat frame;
	int i=0,j=0;
	struct timespec ts;
	ts.tv_sec=1;
	ts.tv_nsec = 1000000;
	if (!cap.isOpened()) {
		printf("cam not open");
		return -1;
	}   

	
//	int contrast   = cap.get(CV_CAP_PROP_CONTRAST );	
//	cout << "Default Contrast----------> "<< contrast << endl;


#if defined(NETWORK_CLIENT)
		int sock_fd;
		int tcp_port = PORT;	
		const char* addr = "192.168.1.46";	
		struct sockaddr_in server_addr;
		
		sock_fd = socket(PF_INET, SOCK_STREAM, 0);
		if(sock_fd < 0){
			printf("==============socket failed");
			exit(-1);
		}
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = inet_addr(addr);
		server_addr.sin_port = htons(tcp_port);
		
	
		if(connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) < 0){
			printf("================connect fail");
			exit(-1);
		}
#endif			

//	cv::namedWindow("Webcam Frame Capture", 1);
//	cv::resizeWindow("Webcam Frame Capture", 640, 480);
	nanosleep(&ts, NULL);
	ts.tv_sec=0;
	
	for (;;) {


		cap >> frame;

		if (frame.empty()) {
			printf("frame empty %d\n", i);
			i++;
			if(i>2)
			break;
		}
		else {
#if defined(NETWORK_CLIENT)
		send(sock_fd, frame.data, frame.total()*2, 0);		
#endif	
			j++;
			printf("frame %d\n", j);
			nanosleep(&ts, NULL);
		//	printf(" %ld %ld %ld \n", frame.total(),frame.elemSize(), frame.total()*frame.elemSize());

		//	cv::imshow("Webcam Frame Capture", frame);
		//	if (cv::waitKey(60) >= 0)
		//		break;
				

			/*if(j==100){
					cap.set(CV_CAP_PROP_CONTRAST, contrast+10);
					contrast   = cap.get(CV_CAP_PROP_CONTRAST );
					cout << "set contrast----------> "<< contrast << endl;
			}
			*/
		}

	}

//	cv::destroyWindow("Webcam Frame Capture");
	return 0;

}
