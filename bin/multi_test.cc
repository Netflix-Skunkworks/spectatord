#include <string>
#include <vector>
#include <curl/curl.h>

#define HANDLECOUNT 10

struct Errbuf {
  char msg[CURL_ERROR_SIZE];
};

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  ((std::string*)userp)->append((char*)contents, size * nmemb);
  return size * nmemb;
}

int main(void) {
  CURLM *multi_handle;
  std::vector<CURL*> handles;
  std::vector<std::string> readBuffers;
  std::vector<Errbuf> errbufs;

  for (int i = 0; i<HANDLECOUNT; i++) {
    handles.emplace_back(curl_easy_init());
    readBuffers.emplace_back(std::string());
    errbufs.emplace_back(Errbuf());

    curl_easy_setopt(handles[i], CURLOPT_URL, "https://example.com");
    curl_easy_setopt(handles[i], CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handles[i], CURLOPT_WRITEDATA, &readBuffers[i]);
    curl_easy_setopt(handles[i], CURLOPT_ERRORBUFFER, errbufs[i].msg);
  }

  printf("handles.size=%zu readBuffers.size=%zu errbufs.size=%zu\n",
         handles.size(), readBuffers.size(), errbufs.size());

  multi_handle = curl_multi_init();

  for (int i = 0; i<HANDLECOUNT; i++)
    curl_multi_add_handle(multi_handle, handles[i]);

  CURLMsg *msg;
  int msgs_left;
  int still_running = 1;

  while (still_running) {
    CURLMcode mc = curl_multi_perform(multi_handle, &still_running);
    printf("== %d still running ==\n", still_running);

    if (still_running)
      /* wait for activity, timeout or "nothing" */
      mc = curl_multi_poll(multi_handle, NULL, 0, 1000, NULL);

    if (mc) {
      printf("== mc break: %d ==\n", mc);
      break;
    }
  }

  while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
    if (msg->msg == CURLMSG_DONE) {
      for (int i = 0; i < HANDLECOUNT; i++) {
        int found = (msg->easy_handle == handles[i]);
        if (found) {
          if (msg->data.result == CURLE_OK) {
            printf("Message from handle %d: transfer ok, size=%zu\n", i, readBuffers[i].size());
            printf("errbuf=%s\n", errbufs[i].msg);
            printf("%s", readBuffers[i].c_str());
          } else {
            printf("Message from handle %d: not ok %d\n", i, msg->data.result);
            printf("errbuf=%s\n", errbufs[i].msg);
          }
        }
      }
    }
  }

  printf("== remove the transfers and cleanup the handles ==\n");
  for (int i = 0; i<HANDLECOUNT; i++) {
    curl_multi_remove_handle(multi_handle, handles[i]);
    curl_easy_cleanup(handles[i]);
  }

  curl_multi_cleanup(multi_handle);

  return 0;
}
