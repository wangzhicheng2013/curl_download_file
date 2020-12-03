#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>
#include <iostream>
#include <curl/curl.h>
namespace curl_utility {
struct curl_data {
    CURL *curl = nullptr;
    char *buffer = nullptr;
    size_t buffer_pos = 0;
    size_t buffer_len = 0;
    int still_running = 0;
};
class curl_download_file {
public:
    curl_download_file() = default;
    virtual ~curl_download_file() {
       curl_multi_remove_handle(multi_handle_, data_.curl);
       curl_easy_cleanup(data_.curl);
       curl_multi_cleanup(multi_handle_);
    }
public:
    bool init(const char *url) {
        multi_handle_ = curl_multi_init();
        if (!multi_handle_) {
            return false;
        }
        data_.curl = curl_easy_init();
        if (!data_.curl) {
            return false;
        }
        curl_easy_setopt(data_.curl, CURLOPT_URL, url);
        curl_easy_setopt(data_.curl, CURLOPT_WRITEDATA, &data_);
        curl_easy_setopt(data_.curl, CURLOPT_VERBOSE, 0L);
        curl_easy_setopt(data_.curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_multi_add_handle(multi_handle_, data_.curl);
        curl_multi_perform(multi_handle_, &(data_.still_running));
        return true;
    }
    bool save_local_file(const char *local_file) {
        FILE *fp_local = fopen(local_file, "wb+");
        if (!fp_local) {
            return false;
        }
        size_t nread = 0;
        static const int BUFF_SIZE = 256;
        char buffer[BUFF_SIZE] = "";
        do {
            nread = url_fread(buffer, 1, sizeof(buffer));
            fwrite(buffer, 1, nread, fp_local);
        } while(nread > 0);
        fclose(fp_local);
        return true;
    }
private:
    static size_t write_callback(const char *buffer, size_t size, size_t nitems, void *userp) {
        curl_data *p_curl_data = static_cast<curl_data *>(userp);
        if (!p_curl_data) {
            return 0;
        }
        size_t total_size = size * nitems;
        size_t remain_size = p_curl_data->buffer_len - p_curl_data->buffer_pos;
        if (total_size > remain_size) {
            size_t new_len = p_curl_data->buffer_len + total_size - remain_size;
            char *new_buffer = (char *)realloc(p_curl_data->buffer,  new_len);
            if (!new_buffer) {
                return 0;
            }
            p_curl_data->buffer_len = new_len;
            p_curl_data->buffer = new_buffer;
        }
        memcpy(p_curl_data->buffer + p_curl_data->buffer_pos, buffer, total_size);
        p_curl_data->buffer_pos += total_size;
        return total_size;
    }
private:
    bool fill_buffer(size_t want) {
        if (!data_.still_running || data_.buffer_pos > want) {
            return false;
        }

        fd_set fd_read;
        fd_set fd_write;
        fd_set fd_excep;
        struct timeval timeout = { 0 };
        
        do {
            long curl_timeo = -1;
            int max_fd = -1;
            int rc  = 0;

            FD_ZERO(&fd_read);
            FD_ZERO(&fd_write);
            FD_ZERO(&fd_excep);

            timeout.tv_sec = 60;
            timeout.tv_usec = 0;

            curl_multi_timeout(multi_handle_, &curl_timeo);
            if (curl_timeo >= 0) {
                timeout.tv_sec = curl_timeo / 1000;
                if (timeout.tv_sec > 1) {
                    timeout.tv_sec = 1;
                }
                else {
                    timeout.tv_usec = (curl_timeo % 1000) * 1000;
                }
            }
            CURLMcode mc = curl_multi_fdset(multi_handle_, &fd_read, &fd_write, &fd_excep, &max_fd);
            if (mc != CURLM_OK) {
                std::cerr << "curl_multi_fdset failed." << std::endl;
                break;
            }
            if (-1 == max_fd) {
                struct timeval wait_time = { 0, 100 * 1000 };
                rc = select(0, nullptr, nullptr, nullptr, &wait_time);
            }
            else {
                rc = select(max_fd + 1, &fd_read, &fd_write, &fd_excep, &timeout);
            }
            switch (rc)
            {
            case -1:
                break;
            case 0:
            default:
                curl_multi_perform(multi_handle_, &data_.still_running);
                break;
            }
        } while (data_.still_running && (data_.buffer_pos < want));
        return true;
    }
    void use_buffer(size_t want) {
        if (data_.buffer_pos <= want) {
            free(data_.buffer);
            data_.buffer = nullptr;
            data_.buffer_pos = 0;
            data_.buffer_len = 0;
            return;
        }
        memmove(data_.buffer, &data_.buffer[want], data_.buffer_pos - want);
        data_.buffer_pos -= want;
    }
    size_t url_fread(void *ptr, size_t size, size_t nmemb) {
        size_t want = size * nmemb;
        fill_buffer(want);
        if (0 == data_.buffer_pos) {
            return 0;
        }
        if (data_.buffer_pos < want) {
            want = data_.buffer_pos;
        }
        memcpy(ptr, data_.buffer, want);
        use_buffer(want);
        want /= size;
        return want;
    }
private:
    CURLM *multi_handle_ = nullptr;
    curl_data data_;
};
}