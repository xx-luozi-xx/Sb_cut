/**
 * @file Sb_cut.cpp
 * @author xx-luozi-xx
 * @brief 吧长截图里的船切单独截出并输出
 *        输入：一张图片
 *        输出：船的图片
 *        基本思想：
 *        对每一列和每一行单独求方差，方差较小的行或者列认为是背景
 *        长截图中列比较长，所以较为准确，找出列背景后求列背景的均值
 *        求行均值时混入一定的背景色再求方差增加识别率。
 * 
 *        找具体船位置时，以三个(不同分辨率动态调节)连续前景像素为船的位置,
 *        强行设置一段(通过比例预先设定好的)前景区间，随后寻找三个(同上)连续背景像素为船的结束。
 * 
 *        最后通过所有船的长度和宽度求均值，重新调整宽度和位置使得输出截图大小一致。
 * 
 *        输出时在最后一行船特判这是否是船或者背景。
 * 
 *        输入输出和日志路径均在config.json中配置。
 * 
 * @version 0.1
 * @date 2024-10-04
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>

#include <chrono>   // for high_resolution_clock

#include "Utils.h"  // for get_timestamp()

using json = nlohmann::json;
using namespace std;
using namespace cv;

string root_folder = "./";

string in_file;
string in_folder;
string out_folder;
string log_folder;
string log_file = "Sb_cut_log.txt";

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
const int BACKGROUND_VAR = 2500;

bool load_config();

int main(int argc, char** argv){
    // 读取命令行参数 为程序所在路径
    if(argc > 1){
        root_folder = argv[1];
    }

    if(load_config() == false){
        std::cout << "Cannot load config file." << endl;
        return -1;
    }

    auto time_start = chrono::system_clock::now();

    // Delete old ships
    {
        string cmd = "powershell.exe Remove-Item -Path \"" + out_folder + "\\*.png\" -Force";
        system(cmd.c_str());
    }

    img = cv::imread(in_folder+in_file, cv::IMREAD_COLOR);
    if(img.empty()){
        cout << "Cannot read image: " << in_folder+in_file << endl;
        return -1;
    }
    
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

    //统一宽度大小矫正
    double avg_width = 0;
    for(auto itr_col = ship_pos_col.begin(); itr_col != ship_pos_col.end(); itr_col++){
        avg_width += itr_col->second;
    }
    avg_width /= ship_pos_col.size();

    int new_width = ceil(avg_width);
    for(auto itr_col = ship_pos_col.begin(); itr_col != ship_pos_col.end(); itr_col++){
        itr_col->first += (itr_col->second - new_width)/2;
        itr_col->second = new_width;
    }

    //统一高度大小矫正
    double avg_height = 0;
    for(auto itr_row = ship_pos_row.begin(); itr_row != ship_pos_row.end(); itr_row++){
        avg_height += itr_row->second;
    }
    avg_height /= ship_pos_row.size();

    int new_height = ceil(avg_height);
    for(auto itr_row = ship_pos_row.begin(); itr_row != ship_pos_row.end(); itr_row++){
        itr_row->first += (itr_row->second - new_height)/2;
        itr_row->second = new_height;
    }


    //裁剪
    int conter = 0;
    for(auto itr_row = ship_pos_row.begin(); itr_row != ship_pos_row.end(); itr_row++){
        for(auto itr_col = ship_pos_col.begin(); itr_col != ship_pos_col.end(); itr_col++){
            Rect rect(itr_col->first, itr_row->first, itr_col->second, itr_row->second);
            Mat sub_img = img(rect);

            //最后一行做空白检测
            auto next_itr_row = itr_row;
            next_itr_row++;
            if(next_itr_row == ship_pos_row.end()){
                Vec3d avg_color(0,0,0);
                Vec3d var_color(0,0,0);
                meanStdDev(sub_img, avg_color, var_color);
                double var = var_color[0] * var_color[0] + var_color[1] * var_color[1] + var_color[2] * var_color[2];
                var /= 3;

                if(var < BACKGROUND_VAR){//一个是空白，剩下的全是空白
                    break;
                }
            }            

            cv::imwrite(out_folder + to_string(conter) + ".png", sub_img);
            conter++;
        }
    }

    auto time_end = chrono::system_clock::now();
    std::chrono::duration<double, std::milli> duration = time_end - time_start;
    cout << "Time used: " << duration.count() << " ms" << endl;

    ofstream log(log_folder + log_file , ios::app);
    log << "Timestamp: " << get_timestamp() << endl;
    log << "Input file: " << in_folder + in_file << endl;
    log << "Ship count: " << conter << endl;
    log << "Time used: " << duration.count() << " ms" << endl;
    log << "-" << endl;
    log.close();

    return 0;
}


bool load_config(){
    json config_json;
    ifstream config_file(root_folder+"config/config.json");
    if(config_file.is_open()){
        config_file >> config_json;
        config_file.close();
    }else{
        cout << "Failed to load config file." << endl;
        return false;
    }

    in_folder = config_json["input_folder"];
    in_file = config_json["input_file"];
    out_folder = config_json["output_folder"];
    log_folder = config_json["log_folder"];
    
    return true;
}
