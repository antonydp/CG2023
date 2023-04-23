#include <iostream>
#include <cstdlib>   // for system() function
#include <string>    // for string class
#include <direct.h>  // for _getcwd() function

using namespace std;

int launch_batch_file() {
    char buffer[FILENAME_MAX];
    _getcwd(buffer, FILENAME_MAX);
    string current_dir(buffer);
    cout << "current file dir is " << current_dir << endl;
    string bat_file_path = current_dir + "\\run_LAB_04.bat";
    int result = system(bat_file_path.c_str());
    if (result == 0) {
        cout << "Batch file executed successfully!" << endl;
    }
    else {
        cout << "Error executing batch file!" << endl;
    }
    return 0;
}