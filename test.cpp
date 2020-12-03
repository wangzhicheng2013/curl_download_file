#include "curl_download_file.hpp"
int main() {
    curl_utility::curl_download_file download_file_utility;
    if (!download_file_utility.init("https://raw.githubusercontent.com/curl/curl/master/docs/examples/fopen.c")) {
        std::cerr << "init failed." << std::endl;
        return -1;
    }
    download_file_utility.save_local_file("./test.file");

    return 0;
}