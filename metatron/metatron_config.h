#pragma once

#include <string>
#include <curl/curl.h>
#include <openssl/ssl.h>

namespace metatron {
  std::tuple<std::string, std::string, std::string, std::string> find_certificate();

  int metatron_verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx);

  CURLcode sslctx_metatron_verify(CURL *curl, void *ssl_ctx, void *parm);
}
