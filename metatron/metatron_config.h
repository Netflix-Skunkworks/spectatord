#pragma once

#include <string>
#include <curl/curl.h>
#include <openssl/ssl.h>

namespace metatron {

struct CertInfo {
  std::string ssl_cert;
  std::string ssl_key;
  std::string ca_info;
  std::string app_name;
};

CertInfo find_certificate(bool external_enabled, const std::string& metatron_dir);

int metatron_verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx);

CURLcode sslctx_metatron_verify(CURL *curl, void *ssl_ctx, void *parm);

}  // namespace metatron
