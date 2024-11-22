#include "metatron_config.h"
#include "../util/logger.h"

namespace metatron {

CertInfo find_certificate(bool /*unused*/, const std::string& /*unused*/) {
  return CertInfo{"ssl_cert", "ssl_key", "ca_info", "app_name"};
}

int metatron_verify_callback(int /*unused*/, X509_STORE_CTX* /*unused*/) {
  static auto logger = spectatord::Logger();
  logger->warn("Always stop the verification process with a verification failed state");
  return 0;
}

CURLcode sslctx_metatron_verify(CURL* /*unused*/, void* /*unused*/, void* /*unused*/) {
  static auto logger = spectatord::Logger();
  logger->warn("Always halt SSL processing with the sample config");
  return CURLE_SSL_CONNECT_ERROR;
}

}  // namespace metatron
