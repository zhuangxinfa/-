﻿#include "shuangmu2.h"
#include <QFileDialog>
#include <QDebug>
#include "opencv2/calib3d.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <QThread>
#include <QMessageBox>
#include <qvariant.h>

//qt加opencv打开摄像头using namespace cv;
using namespace std;
using namespace cv;

int countCameras()
{
	cv::VideoCapture temp_camera;
	int maxTested = 10;
	for (int i = 0; i < maxTested; i++) {
		cv::VideoCapture temp_camera(i);
		bool res = (!temp_camera.isOpened());
		temp_camera.release();
		if (res)
		{
			return i;
		}
	}
	return maxTested;
}
int camera_num = countCameras();

VideoCapture capture(0);
VideoCapture capture2(1);

static int print_help()
{
	cout <<
		" Given a list of chessboard images, the number of corners (nx, ny)\n"
		" on the chessboards, and a flag: useCalibrated for \n"
		"   calibrated (0) or\n"
		"   uncalibrated \n"
		"     (1: use cvStereoCalibrate(), 2: compute fundamental\n"
		"         matrix separately) stereo. \n"
		" Calibrate the cameras and display the\n"
		" rectified results along with the computed disparity images.   \n" << endl;
	cout << "Usage:\n ./stereo_calib -w=<board_width default=9> -h=<board_height default=6> -s=<square_size default=1.0> <image list XML/YML file default=../data/stereo_calib.xml>\n" << endl;
	return 0;
}


static Mat StereoCalib( const vector<std::string>& imagelist, Size boardSize, float squareSize,  Ui::shuangmu2Class ui, bool displayCorners = true, bool useCalibrated = true, bool showRectified = true )
{
	//VideoCapture capture(1);
	//VideoCapture capture2(2);
	

	const int maxScale = 2;
	// ARRAY AND VECTOR STORAGE:

	vector<vector<Point2f> > imagePoints[2];
	vector<vector<Point3f> > objectPoints;
	Size imageSize;

	int i, j, k, nimages = (int)imagelist.size() / 2;

	imagePoints[0].resize(nimages);
	imagePoints[1].resize(nimages);
	vector<string> goodImageList;
	cout << "the num of image is :" << nimages << endl;
	for (i = j = 0; i < nimages; i++)
	{
		ui.progressBar->setValue(i*7);
		for (k = 0; k < 2; ++k)
		{
			cout << "k is :" << k << endl;
			const string& filename = imagelist[i * 2 + k];
			cout << filename << endl;
			Mat img = imread(filename, 0);
			if (img.empty())
			{
				cout << "image is empty" << endl;
				break;
			}
			if (imageSize == Size())
				imageSize = img.size();
			else if (img.size() != imageSize)
			{
				cout << "The image " << filename << " has the size different from the first image size. Skipping the pair\n";
				break;
			}
			bool found = false;
			vector<Point2f>& corners = imagePoints[k][j];
			for (int scale = 1; scale <= maxScale; scale++)
			{
				Mat timg;
				if (scale == 1)
					timg = img;
				else
					cv::resize(img, timg, Size(), scale, scale);
				found = findChessboardCorners(timg, boardSize, corners,
					CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_NORMALIZE_IMAGE);
				if (found)
				{
					if (scale > 1)
					{
						Mat cornersMat(corners);
						cornersMat *= 1. / scale;
					}

					break;
				}
			}

			if (0)
			{
				cout << filename << endl;
				Mat cimg, cimg1;
				cvtColor(img, cimg, COLOR_GRAY2BGR);
				drawChessboardCorners(cimg, boardSize, corners, found);
				double sf = 640. / MAX(img.rows, img.cols);
				cv::resize(cimg, cimg1, Size(), sf, sf);
				imshow("corners", cimg1);
				char c = (char)waitKey();
				//char c = (char)waitKey(500);
				if (c == 27 || c == 'q' || c == 'Q') //Allow ESC to quit
					exit(-1);
			}
			else
				putchar('.');

			if (!found)
			{
				cout << "found" << endl;
				break;
			}
			cornerSubPix(img, corners, Size(11, 11), Size(-1, -1),
				TermCriteria(TermCriteria::COUNT + TermCriteria::EPS,
					30, 0.01));

		}
		if (k == 2)
		{
			cout << "k == 2" << endl;
			goodImageList.push_back(imagelist[i * 2]);
			goodImageList.push_back(imagelist[i * 2 + 1]);
			j++;
		}
	}
	cout << j << " pairs have been successfully detected.\n";
	nimages = j;
	

	imagePoints[0].resize(nimages);
	imagePoints[1].resize(nimages);
	objectPoints.resize(nimages);

	for (i = 0; i < nimages; i++)
	{
		for (j = 0; j < boardSize.height; j++)
			for (k = 0; k < boardSize.width; k++)
				objectPoints[i].push_back(Point3f(k*squareSize, j*squareSize, 0));
	}

	cout << "Running stereo calibration ...\n";

	Mat cameraMatrix[2], distCoeffs[2];
	cameraMatrix[0] = initCameraMatrix2D(objectPoints, imagePoints[0], imageSize, 0);
	cameraMatrix[1] = initCameraMatrix2D(objectPoints, imagePoints[1], imageSize, 0);
	Mat R, T, E, F;

	double rms = stereoCalibrate(objectPoints, imagePoints[0], imagePoints[1],
		cameraMatrix[0], distCoeffs[0],
		cameraMatrix[1], distCoeffs[1],
		imageSize, R, T, E, F, CALIB_ZERO_TANGENT_DIST +
		CALIB_FIX_K3 + CALIB_FIX_K4 + CALIB_FIX_K5 +
		CALIB_USE_INTRINSIC_GUESS,
		TermCriteria(TermCriteria::COUNT + TermCriteria::EPS, 100, 1e-5));
	/*CALIB_FIX_ASPECT_RATIO +
	CALIB_ZERO_TANGENT_DIST +
	CALIB_USE_INTRINSIC_GUESS +
	CALIB_SAME_FOCAL_LENGTH +
	CALIB_RATIONAL_MODEL +
	CALIB_FIX_K3 + CALIB_FIX_K4 + CALIB_FIX_K5,*/
	cout << "done with RMS error=" << rms << endl;

	// CALIBRATION QUALITY CHECK
	// because the output fundamental matrix implicitly
	// includes all the output information,
	// we can check the quality of calibration using the
	// epipolar geometry constraint: m2^t*F*m1=0
	double err = 0;
	int npoints = 0;
	vector<Vec3f> lines[2];
	for (i = 0; i < nimages; i++)
	{
		
		int npt = (int)imagePoints[0][i].size();
		Mat imgpt[2];
		for (k = 0; k < 2; k++)
		{
			imgpt[k] = Mat(imagePoints[k][i]);
			undistortPoints(imgpt[k], imgpt[k], cameraMatrix[k], distCoeffs[k], Mat(), cameraMatrix[k]);
			computeCorrespondEpilines(imgpt[k], k + 1, F, lines[k]);
		}
		for (j = 0; j < npt; j++)
		{
			double errij = fabs(imagePoints[0][i][j].x*lines[1][j][0] +
				imagePoints[0][i][j].y*lines[1][j][1] + lines[1][j][2]) +
				fabs(imagePoints[1][i][j].x*lines[0][j][0] +
					imagePoints[1][i][j].y*lines[0][j][1] + lines[0][j][2]);
			err += errij;
		}
		npoints += npt;
	}
	cout << "average epipolar err = " << err / npoints << endl;

	// save intrinsic parameters
	FileStorage fs("intrinsics.yml", FileStorage::WRITE);
	if (fs.isOpened())
	{
		fs << "M1" << cameraMatrix[0] << "D1" << distCoeffs[0] <<
			"M2" << cameraMatrix[1] << "D2" << distCoeffs[1];
		fs.release();
	}
	else
		cout << "Error: can not save the intrinsic parameters\n";

	Mat R1, R2, P1, P2, Q;
	Rect validRoi[2];

	stereoRectify(cameraMatrix[0], distCoeffs[0],
		cameraMatrix[1], distCoeffs[1],
		imageSize, R, T, R1, R2, P1, P2, Q,
		CALIB_ZERO_DISPARITY, 0, imageSize, &validRoi[0], &validRoi[1]);

	fs.open("extrinsics.yml", FileStorage::WRITE);
	if (fs.isOpened())
	{
		fs << "R" << R << "T" << T << "R1" << R1 << "R2" << R2 << "P1" << P1 << "P2" << P2 << "Q" << Q;
		fs.release();
	}
	else
		cout << "Error: can not save the extrinsic parameters\n";

	// OpenCV can handle left-right
	// or up-down camera arrangements
	bool isVerticalStereo = fabs(P2.at<double>(1, 3)) > fabs(P2.at<double>(0, 3));

	// COMPUTE AND DISPLAY RECTIFICATION
	

	Mat rmap[2][2];
	// IF BY CALIBRATED (BOUGUET'S METHOD)
	if (useCalibrated)
	{
		// we already computed everything
	}
	// OR ELSE HARTLEY'S METHOD
	else
		// use intrinsic parameters of each camera, but
		// compute the rectification transformation directly
		// from the fundamental matrix
	{
		vector<Point2f> allimgpt[2];
		for (k = 0; k < 2; k++)
		{
			for (i = 0; i < nimages; i++)
				std::copy(imagePoints[k][i].begin(), imagePoints[k][i].end(), back_inserter(allimgpt[k]));
		}
		F = findFundamentalMat(Mat(allimgpt[0]), Mat(allimgpt[1]), FM_8POINT, 0, 0);
		Mat H1, H2;
		stereoRectifyUncalibrated(Mat(allimgpt[0]), Mat(allimgpt[1]), F, imageSize, H1, H2, 3);

		R1 = cameraMatrix[0].inv()*H1*cameraMatrix[0];
		R2 = cameraMatrix[1].inv()*H2*cameraMatrix[1];
		P1 = cameraMatrix[0];
		P2 = cameraMatrix[1];
	}

	//Precompute maps for cv::remap()
	initUndistortRectifyMap(cameraMatrix[0], distCoeffs[0], R1, P1, imageSize, CV_16SC2, rmap[0][0], rmap[0][1]);
	initUndistortRectifyMap(cameraMatrix[1], distCoeffs[1], R2, P2, imageSize, CV_16SC2, rmap[1][0], rmap[1][1]);

	Mat canvas;

	double sf;
	int w, h;
	if (!isVerticalStereo)
	{
		sf = 600. / MAX(imageSize.width, imageSize.height);
		w = cvRound(imageSize.width*sf);
		h = cvRound(imageSize.height*sf);
		canvas.create(h, w * 2, CV_8UC3);//创建一个空白的图像
	}
	else
	{
		sf = 300. / MAX(imageSize.width, imageSize.height);
		w = cvRound(imageSize.width*sf);
		h = cvRound(imageSize.height*sf);
		canvas.create(h * 2, w, CV_8UC3);
	}
	//nimages
	//for (i = 0; i < nimages; i++)
	//{
	while (true)
	{


		for (k = 0; k < 2; k++)//读取左右摄像头
		{
			Mat img, rimg, cimg;

			if (k % 2 == 0) {
				
				//capture.open(1);
				capture.read(img);
				cvtColor(img, img, COLOR_BGR2GRAY);
			}
			else {
				//capture2.open(2);
				capture2.read(img);
				cvtColor(img, img, COLOR_BGR2GRAY);
			}
			//Mat img = imread("C:\\Users\\zhuan\\Desktop\\stero.jpg",0), rimg, cimg;
			//Mat img = imread(goodImageList[i * 2 + k], 0), rimg, cimg;
			//imshow("test", cimg);
			remap(img, rimg, rmap[k][0], rmap[k][1], INTER_LINEAR);
			cvtColor(rimg, cimg, COLOR_GRAY2BGR);
			Mat canvasPart = !isVerticalStereo ? canvas(Rect(w*k, 0, w, h)) : canvas(Rect(0, h*k, w, h));
			//imshow("rectified", canvas);
			cv::resize(cimg, canvasPart, canvasPart.size(), 0, 0, INTER_AREA);
			
			//imshow("test", rimg);
			if (useCalibrated)
			{
				Rect vroi(cvRound(validRoi[k].x*sf), cvRound(validRoi[k].y*sf),
					cvRound(validRoi[k].width*sf), cvRound(validRoi[k].height*sf));
				rectangle(canvasPart, vroi, Scalar(0, 0, 255), 3, 8);
			}
		}
		
		return canvas;
		//imshow("rectified", canvas);
		/*if (!isVerticalStereo)
		for (j = 0; j < canvas.rows; j += 16)
		line(canvas, Point(0, j), Point(canvas.cols, j), Scalar(0, 255, 0), 1, 8);
		else
		for (j = 0; j < canvas.cols; j += 16)
		line(canvas, Point(j, 0), Point(j, canvas.rows), Scalar(0, 255, 0), 1, 8);*/
		//imshow("rectified", canvas);
		/*char c = (char)waitKey();
		if (c == 27 || c == 'q' || c == 'Q')
		break;*/
		//waitKey(10);
	}
}
static bool readStringList(const string& filename, vector<string>& l)
{
	l.resize(0);
	FileStorage fs(filename, FileStorage::READ);
	if (!fs.isOpened())
		return false;
	FileNode n = fs.getFirstTopLevelNode();
	if (n.type() != FileNode::SEQ)
		return false;
	FileNodeIterator it = n.begin(), it_end = n.end();
	for (; it != it_end; ++it)
		l.push_back((string)*it);
	return true;
}
void shuangmu2::left_comBoBox()
{
	qDebug() <<"left";
	qDebug() << ui.comboBox_2->currentIndex();
	if (ui.comboBox_2->currentIndex()>=camera_num){
		QMessageBox::information(NULL, "warning", "This device doesn't have so many cameras.", QMessageBox::Yes, QMessageBox::Yes);
	}
	else {
		if (ui.comboBox_2->currentIndex() != this->right_camera) {

			//ui.comboBox->setItemData(ui.comboBox_2->currentIndex(),  Qt::UserRole - 1);
			//ui.comboBox->setItemData(2, Qt::lightGray, Qt::BackgroundColorRole);
			this->left_camera = ui.comboBox_2->currentIndex();
			capture.open(this->left_camera);
		}
		else
		{
			QMessageBox::information(NULL, "warning", "Don't turn on the same camera at the same time", QMessageBox::Yes, QMessageBox::Yes);
		}
	}
}

void shuangmu2::right_comBoBox() {
	qDebug() << "right";
	qDebug() << ui.comboBox->currentIndex();
	if (ui.comboBox->currentIndex() >= camera_num) {
		QMessageBox::information(NULL, "warning", "This device doesn't have so many cameras.", QMessageBox::Yes, QMessageBox::Yes);
	}
	else {
		if (this->left_camera != ui.comboBox->currentIndex()) {
			this->right_camera = ui.comboBox->currentIndex();
			capture2.open(this->right_camera);
		}

		else
		{
			QMessageBox::information(NULL, "warning", "Don't turn on the same camera at the same time", QMessageBox::Yes, QMessageBox::Yes);
		}
	}
}
Mat  shuangmu2::main_x()
{
	
	Size boardSize;
	string imagelistfn;
	bool showRectified;
	//cv::CommandLineParser parser(argc, argv, "{w|9|}{h|6|}{s|1.0|}{nr||}{help||}{@input|../data/stereo_calib.xml|}");
	/*if (parser.has("help"))
	return print_help();*/
	//showRectified = !parser.has("nr");
	showRectified = true;
	//imagelistfn = parser.get<string>("@input");
	imagelistfn = "./data/stereo_calib.xml";
	//boardSize.width = parser.get<int>("w");
	boardSize.width = 9;
	//boardSize.height = parser.get<int>("h");
	//float squareSize = parser.get<float>("s");
	boardSize.height = 6;
	float squareSize = 1.0;
	/*if (!parser.check())
	{
	parser.printErrors();
	return 1;
	}*/
	vector<string> imagelist;
	bool ok = readStringList(imagelistfn, imagelist);
	
	
	return  StereoCalib(imagelist, boardSize, squareSize,ui, false, true,true);
	//system("pause");
	//char c;
	//cin >> c;
	
}
//int main1(int argc, char** argv)
//{
//	Size boardSize;
//	string imagelistfn;
//	bool showRectified;
//	cv::CommandLineParser parser(argc, argv, "{w|9|}{h|6|}{s|1.0|}{nr||}{help||}{@input|../data/stereo_calib.xml|}");
//	if (parser.has("help"))
//		return print_help();
//	showRectified = !parser.has("nr");
//	imagelistfn = parser.get<string>("@input");
//	boardSize.width = parser.get<int>("w");
//	boardSize.height = parser.get<int>("h");
//	float squareSize = parser.get<float>("s");
//	if (!parser.check())
//	{
//		parser.printErrors();
//		return 1;
//	}
//	vector<string> imagelist;
//	bool ok = readStringList(imagelistfn, imagelist);
//	if (!ok || imagelist.empty())
//	{
//		cout << "can not open " << imagelistfn << " or the string list is empty" << endl;
//		return print_help();
//	}
//
//	//StereoCalib(imagelist, boardSize, squareSize, false, true, showRectified);
//	//system("pause");
//	//char c;
//	//cin >> c;
//	return 0;
//}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Mat img;
Mat img0;
shuangmu2::shuangmu2(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
//	connect(ui.pushButton, SIGNAL(clicked()), this, SLOT(show_video()));
	connect(ui.pushButton2, SIGNAL(clicked()), this, SLOT(show_video2()));
	connect(ui.pushButton3,SIGNAL(clicked()), this, SLOT(calib()));
	QObject::connect(&thread, SIGNAL(send(int)), this, SLOT(accept(int)));

	ui.progressBar->setRange(0, 100);
	ui.comboBox_2->setCurrentIndex(0);
	ui.comboBox->setCurrentIndex(1);
	ui.progressBar->setValue(0);

	ui.label->setFrameShape(QFrame::Box);
	ui.label->setStyleSheet("border-width: 1px;border-style: solid;border-color: rgb(255, 170, 0);");
	
	ui.label2->setFrameShape(QFrame::Box);
	ui.label2->setStyleSheet("border-width: 1px;border-style: solid;border-color: rgb(255, 170, 0);");
	
	ui.label3->setFrameShape(QFrame::Box);
	ui.label3->setStyleSheet("border-width: 1px;border-style: solid;border-color: rgb(255, 170, 0);");

}
void shuangmu2::calib() {

		//thread.stop();
		Mat a = main_x();
		cvtColor(a, a, COLOR_BGR2RGB);
		img3 = QImage((const unsigned char*)a.data, a.cols, a.rows, QImage::Format_RGB888);
		img3 = img3.scaled(ui.label3->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		ui.label3->setPixmap(QPixmap::fromImage(img3));
		ui.label3->show();
		ui.progressBar->setValue(100);
		waitKey(5);

	
}
//void shuangmu2::show_video() {
//
//	/*QString fileName = QFileDialog::getOpenFileName(this, "open image file", ".", "Image files(*.jpg *.png)");
//	if (fileName != "")
//	{
//		if (image->load(fileName))
//		{
//			ui.label->setPixmap(QPixmap::fromImage(*image));
//			ui.label->show();
//		}
//	}*/
//	//img = cv::imread("‪C:\\Users\\zhuan\\Desktop\\1.jpg");
//	qDebug() << QString("hello");
//	int i = 0;
//	while (i<100000) {
//		if (i % 2 == 0) {
//			//capture.open(left_camera);
//			capture >> img;
//			cvtColor(img, img, COLOR_BGR2RGB);
//			img2 = QImage((const unsigned char*)img.data, img.cols, img.rows, QImage::Format_RGB888);
//			img2 = img2.scaled(ui.label->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
//			ui.label->setPixmap(QPixmap::fromImage(img2));
//			ui.label->show();
//			waitKey(5);
//		}
//		else
//		{
//			//capture2.open(right_camera);
//			capture2 >> img0;
//			cvtColor(img0, img0, COLOR_BGR2RGB);
//			img3 = QImage((const unsigned char*)img0.data, img0.cols, img0.rows, QImage::Format_RGB888);
//			img3 = img3.scaled(ui.label2->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
//			ui.label2->setPixmap(QPixmap::fromImage(img3));
//			ui.label2->show();
//			waitKey(5);
//		}
//		i++;
//	}
//
//}
void shuangmu2::accept(int  i)
{
	
	//qDebug() << QString ("accept ok");
	
	
		if (i % 2 == 0) {
			//capture.open(left_camera);
			capture >> img;
			cvtColor(img, img, COLOR_BGR2RGB);
			img2 = QImage((const unsigned char*)img.data, img.cols, img.rows, QImage::Format_RGB888);
			img2 = img2.scaled(ui.label->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			ui.label->setPixmap(QPixmap::fromImage(img2));
			ui.label->show();
			waitKey(5);
		}
		else
		{
			//capture2.open(right_camera);
			capture2 >> img0;
			cvtColor(img0, img0, COLOR_BGR2RGB);
			img3 = QImage((const unsigned char*)img0.data, img0.cols, img0.rows, QImage::Format_RGB888);
			img3 = img3.scaled(ui.label2->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			ui.label2->setPixmap(QPixmap::fromImage(img3));
			ui.label2->show();
			waitKey(5);
		}
		
	
	
		
	
}
void shuangmu2::show_video2() {  
	
	thread.start();
	qDebug() << QString("thread start");
	
	/*QString fileName = QFileDialog::getOpenFileName(this, "open image file", ".", "Image files(*.jpg *.png)");
	if (fileName != "")
	{
	if (image->load(fileName))
	{
	ui.label->setPixmap(QPixmap::fromImage(*image));
	ui.label->show();
	}
	}*/
	//img = cv::imread("‪C:\\Users\\zhuan\\Desktop\\1.jpg");
	/*while (1) {

		capture2 >> img0;
		cvtColor(img0, img0, COLOR_BGR2RGB);
		img3 = QImage((const unsigned char*)img0.data, img0.cols, img0.rows, QImage::Format_RGB888);
		img3 = img3.scaled(ui.label2->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		ui.label2->setPixmap(QPixmap::fromImage(img3));
		ui.label2->show();
		waitKey(30);
	}*/

}
shuangmu2::~shuangmu2()
{

}
