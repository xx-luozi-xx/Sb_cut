#include <fstream>
#include <iostream>
#include <string>
#include <vector>  

#include <opencv2/opencv.hpp>

#include "Setting.h"
#include "Utils.h"

using namespace cv;
using namespace std;

string data_path = root_data_path;
string image_floder = data_path + "out/sub/";
string log_folder = data_path + "out/log/";

const int front_image_begin = 0;
const int front_image_end = 583;
vector<string> back_image_list = {"584.png", "585.png", "586.png", "587.png"};

int main(){
    double avg_var_front = 0;
    int counter = 0;
    for(int i = front_image_begin; i <= front_image_end; i++){
        string front_image_path = image_floder + to_string(i) + ".png";
        Mat front_image = imread(front_image_path, CV_32SC3);
        if(front_image.empty()){
            cout << "Error: " << front_image_path << " not found." << endl;
            continue;
        }
        Vec3d mean_front, stddev_front;
        meanStdDev(front_image, mean_front, stddev_front);
        double var_front = stddev_front[0] * stddev_front[0] + stddev_front[1] * stddev_front[1] + stddev_front[2] * stddev_front[2];
        var_front /= 3;
        avg_var_front += var_front;
        counter++;
    }
    avg_var_front /= counter;

    double avg_var_back = 0;
    counter = 0;
    for(int i = 0; i < back_image_list.size(); i++){
        string back_image_path = image_floder + back_image_list[i];
        Mat back_image = imread(back_image_path, CV_32SC3);
        if(back_image.empty()){
            cout << "Error: " << back_image_path << " not found." << endl;
            continue;
        }
        Vec3d mean_back, stddev_back;
        meanStdDev(back_image, mean_back, stddev_back);
        double var_back = stddev_back[0] * stddev_back[0] + stddev_back[1] * stddev_back[1] + stddev_back[2] * stddev_back[2];
        var_back /= 3;
        avg_var_back += var_back;
        counter++;
    }
    avg_var_back /= counter;

    cout << "Front avg var: " << avg_var_front << endl;
    cout << "Back avg var: " << avg_var_back << endl;

    ofstream log_file(log_folder + "var.txt", ios::app);
    log_file << "Timestamp: " << get_timestamp() << endl;
    log_file << "Front avg var: " << avg_var_front << endl;
    log_file << "Back avg var: " << avg_var_back << endl;

    return 0;
}


