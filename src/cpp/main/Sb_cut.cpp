#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

#include <chrono>

#include "Setting.h" 

using namespace std;
using namespace cv;

string data_path = root_data_path;
string in_folder = data_path + "in/";
string out_folder = data_path + "out/sub/";
string log_folder = data_path + "out/log/";
string in_file = in_folder + "cr2.png";
string out_file = log_folder + "cr2_out.png";
cv::Mat img;

const double WIDTH = 1280.0;
int ROW_BEGIN_POSITION(){
    return 75/WIDTH*img.cols;
} 
int WIDTH_OF_SHIP(){
    return 142/WIDTH*img.cols;
}
int HIGH_OF_SHIP(){
    return 205/WIDTH*img.cols;
}
int HAVING_COLOR(){
    return max(1.0,3/WIDTH*img.cols);
}

const int THRESHOLD = 3;

int main(){

    auto time_start = chrono::system_clock::now();

    img = cv::imread(in_file, cv::IMREAD_COLOR);
    if(img.empty()){
        cout << "Cannot read image: " << in_file << endl;
        return -1;
    }
    // cv::namedWindow("Input Image", cv::WINDOW_AUTOSIZE);
    // cv::imshow("Input Image", img);
    // cv::waitKey(0);

    // 算每列均值
    vector<Vec3d> avg_colors_col(img.cols, Vec3d(0,0,0));
    for(int j = 0; j < img.cols; j++){
        for(int i = 0; i < img.rows; i++){
            avg_colors_col[j][0] += img.at<cv::Vec3b>(i,j)[0];
            avg_colors_col[j][1] += img.at<cv::Vec3b>(i,j)[1];
            avg_colors_col[j][2] += img.at<cv::Vec3b>(i,j)[2];
        }
    }
    for(int j = 0; j < avg_colors_col.size(); j++){
        avg_colors_col[j][0] /= img.rows;
        avg_colors_col[j][1] /= img.rows;
        avg_colors_col[j][2] /= img.rows;
    }

    // 算每列方差
    vector<Vec3d> var_colors_col(img.cols, Vec3d(0,0,0));
    for(int j = 0; j < img.cols; j++){
        for(int i = 0; i < img.rows; i++){
            var_colors_col[j][0] += pow(img.at<cv::Vec3b>(i,j)[0] - avg_colors_col[j][0], 2);
            var_colors_col[j][1] += pow(img.at<cv::Vec3b>(i,j)[1] - avg_colors_col[j][1], 2);
            var_colors_col[j][2] += pow(img.at<cv::Vec3b>(i,j)[2] - avg_colors_col[j][2], 2);
        }
    }
    for(int j = 0; j < var_colors_col.size(); j++){
        var_colors_col[j][0] /= img.rows;
        var_colors_col[j][1] /= img.rows;
        var_colors_col[j][2] /= img.rows;
    }


    // 3通道方差均值到1通道
    vector<double> var_col(img.cols, 0);
    for(int j = 0; j < img.cols; j++){
        var_col[j] = var_colors_col[j][0] + var_colors_col[j][1] + var_colors_col[j][2];
        var_col[j] /= 3;
    }


    // 随便找个阈值
    double max_var_col = *max_element(var_col.begin(), var_col.end());
    double min_var_col = *min_element(var_col.begin(), var_col.end());
    double stresh_var_col = (max_var_col + min_var_col) / THRESHOLD;

    // 每个船的列做强制阈值处理，顺便寻找角点
    vector<pair<int, int>> ship_pos_col;//[x_begin, x_length]
    int col_begin = 0;
    for(int j_begin = 0; j_begin < img.cols; j_begin = col_begin){
        int front_cols = 0;
        col_begin = 0;
        for(int j = j_begin; j < img.cols; j++){
            if(var_col[j] >= stresh_var_col){
                if(col_begin == 0){
                    col_begin = j;
                }            
                front_cols++;
            }else{
                front_cols = 0;
                col_begin = 0;
            }
            if(front_cols >= HAVING_COLOR()){
                break;
            }
        }
        if(front_cols < HAVING_COLOR()){//没有找到下一个船，退出
            break;
        }
        if(img.cols - col_begin < WIDTH_OF_SHIP()){//到尾了，退出
            // 余下部分全为背景
            for(int j = col_begin; j < img.cols; j++){
                var_col[j] = 0;
            }
            break;
        }

        pair<int, int> ship_pos;//[x_begin, x_length]
        ship_pos.first = col_begin;
        for(int j = col_begin; j < col_begin+ WIDTH_OF_SHIP(); j++){
            var_col[j] = max_var_col;
        }

        int back_cols = 0;
        for(int j = col_begin+WIDTH_OF_SHIP();j < img.cols; j++){
            if(var_col[j] < stresh_var_col){
                if(back_cols == 0){
                    col_begin = j;
                }
                back_cols ++;
            }else{
                back_cols = 0;
                col_begin = 0;
            }

            if(back_cols >= HAVING_COLOR()){
                break;
            }
        }
        ship_pos.second = col_begin - ship_pos.first;
        ship_pos_col.push_back(ship_pos);
    }

    // 算背景平均颜色
    Vec3d avg_back_color(0,0,0);
    long long int count_back_color = 0;
    for(int j = 0; j < img.cols; j++){
        if(j < WIDTH_OF_SHIP() || img.cols - j < WIDTH_OF_SHIP()){
            continue;
        }
        if(var_col[j] < stresh_var_col){
            for(int i = 0; i < img.rows; i++){
                avg_back_color[0] += img.at<cv::Vec3b>(i,j)[0];
                avg_back_color[1] += img.at<cv::Vec3b>(i,j)[1];
                avg_back_color[2] += img.at<cv::Vec3b>(i,j)[2];
                count_back_color++;
            }
        }
    }
    avg_back_color[0] /= count_back_color;
    avg_back_color[1] /= count_back_color;
    avg_back_color[2] /= count_back_color;



    //算每行均值, 混入一些背景颜色作为区分
    vector<Vec3d> avg_colors_row(img.rows, Vec3d(0,0,0));
    for(int i = 0; i < img.rows; i++){
        for(int j = 0; j < img.cols; j++){
            avg_colors_row[i][0] += img.at<cv::Vec3b>(i,j)[0];
            avg_colors_row[i][1] += img.at<cv::Vec3b>(i,j)[1];
            avg_colors_row[i][2] += img.at<cv::Vec3b>(i,j)[2];
        }
        avg_colors_row[i][0] += avg_back_color[0]*(img.cols/2); 
        avg_colors_row[i][1] += avg_back_color[1]*(img.cols/2); 
        avg_colors_row[i][2] += avg_back_color[2]*(img.cols/2); 
    }
    for(int i = 0; i < avg_colors_row.size(); i++){
        avg_colors_row[i][0] /= img.cols*1.5;
        avg_colors_row[i][1] /= img.cols*1.5;
        avg_colors_row[i][2] /= img.cols*1.5;
    }

    //算每行方差
    vector<Vec3d> var_colors_row(img.rows, Vec3d(0,0,0));
    for(int i = 0; i < img.rows; i++){
        for(int j = 0; j < img.cols; j++){
            var_colors_row[i][0] += pow(img.at<cv::Vec3b>(i,j)[0] - avg_colors_row[i][0], 2);
            var_colors_row[i][1] += pow(img.at<cv::Vec3b>(i,j)[1] - avg_colors_row[i][1], 2);
            var_colors_row[i][2] += pow(img.at<cv::Vec3b>(i,j)[2] - avg_colors_row[i][2], 2);
        }
    }
    for(int i = 0; i < var_colors_row.size(); i++){
        var_colors_row[i][0] /= img.cols;
        var_colors_row[i][1] /= img.cols;
        var_colors_row[i][2] /= img.cols;
    }

    //3通道方差均值到1通道
    vector<double> var_row(img.rows, 0);
    for(int i = 0; i < img.rows; i++){
        if(i < ROW_BEGIN_POSITION()){//初始边界背景
            var_row[i] = 0;
        }else{
            var_row[i] = var_colors_row[i][0] + var_colors_row[i][1] + var_colors_row[i][2];
            var_row[i] /= 3;
        }
    }
    //随便找个阈值
    double max_var_row = *max_element(var_row.begin(), var_row.end());
    double min_var_row = *min_element(var_row.begin(), var_row.end());
    double stresh_var_row = (max_var_row + min_var_row) / THRESHOLD;

    //每个船的行做强制阈值处理
    vector<pair<int, int>> ship_pos_row;//[y_begin, y_length]
    int row_begin = 0;
    for(int i_begin = 0; i_begin < img.rows; i_begin = row_begin){
        int front_rows = 0;
        row_begin = 0;
        for(int i = i_begin; i < img.rows; i++){
            if(var_row[i] >= stresh_var_row){
                if(row_begin == 0){
                    row_begin = i;
                }            
                front_rows++;
            }else{
                front_rows = 0;
                row_begin = 0;
            }
            if(front_rows >= HAVING_COLOR()){
                break;
            }
        }
        if(front_rows < HAVING_COLOR()){//没有找到下一个船，退出
            break;
        }
        if(img.rows - row_begin < HIGH_OF_SHIP()){//到尾了，退出
            //余下部分全为背景
            for(int i = row_begin; i < img.rows; i++){
                var_row[i] = 0;
            }
            break;
        }

        pair<int, int> ship_pos;//[y_begin, y_length]
        ship_pos.first = row_begin;
        for(int i = row_begin; i < row_begin+HIGH_OF_SHIP(); i++){
            var_row[i] = max_var_row;
        }

        int back_rows = 0;
        for(int i = row_begin+HIGH_OF_SHIP();i < img.rows; i++){
            if(var_row[i] < stresh_var_row){
                if(back_rows == 0){
                    row_begin = i;
                }
                back_rows ++;
            }else{
                back_rows = 0;
                row_begin = 0;
            }

            if(back_rows >= HAVING_COLOR()){
                break;
            }
        }
        ship_pos.second = row_begin - ship_pos.first;
        ship_pos_row.push_back(ship_pos);
        
    }


    //裁剪
    int conter = 0;
    for(auto itr_row = ship_pos_row.begin(); itr_row != ship_pos_row.end(); itr_row++){
        for(auto itr_col = ship_pos_col.begin(); itr_col != ship_pos_col.end(); itr_col++){
            Rect rect(itr_col->first, itr_row->first, itr_col->second, itr_row->second);
            Mat sub_img = img(rect);
            cv::imwrite(out_folder + to_string(conter) + ".png", sub_img);
            conter++;
        }
    }

    auto time_end = chrono::system_clock::now();
    std::chrono::duration<double, std::milli> duration = time_end - time_start;
    cout << "Time used: " << duration.count() << " ms" << endl;

    exit(0);

    //前后景二值化
    for(int j = 0; j < img.cols; j++){
        if(var_col[j] < stresh_var_col){
            var_col[j] = 0;
        }else{
            var_col[j] = 1e9;
        }
    }
    for(int i = 0; i < img.rows; i++){
        if(var_row[i] < stresh_var_row){
            var_row[i] = 0;
        }else{
            var_row[i] = 1e9;
        }
    }  



    for(int j = 0; j < img.cols; j++){
        if(var_col[j] < stresh_var_col){
            for(int i = 0; i < img.rows; i++){
                img.at<cv::Vec3b>(i,j)[0] = 0;
                img.at<cv::Vec3b>(i,j)[1] = 0;
                img.at<cv::Vec3b>(i,j)[2] = 0;
            }
        }
    }

    for(int i = 0; i < img.rows; i++){
        if(var_row[i] < stresh_var_row){
            for(int j = 0; j < img.cols; j++){
                img.at<cv::Vec3b>(i,j)[0] = 0;
                img.at<cv::Vec3b>(i,j)[1] = 0;
                img.at<cv::Vec3b>(i,j)[2] = 0;
            }
        }
    }   

    cv::namedWindow("Output Image", cv::WINDOW_AUTOSIZE);
    cv::imshow("Output Image", img);
    cv::waitKey(0);

    {//save output image
        cv::imwrite(out_file, img);
    }

    {//test
        string var_col_log = log_folder + "var_col.txt";
        ofstream ofs(var_col_log.c_str());
        for(int j = 0; j < var_col.size(); j++){
            ofs << j << " " << var_col[j] << endl;
        }
        ofs.close();

        string var_row_log = log_folder + "var_row.txt";    
        ofs.open(var_row_log.c_str());
        for(int i = 0; i < var_row.size(); i++){
            ofs << i << " " << var_row[i] ;
            if(var_row[i] >= stresh_var_row){
                ofs << "[1]";
            }
            ofs << endl;
        }
        ofs.close();
    }
    return 0;
}