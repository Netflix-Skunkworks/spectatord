load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def nflx_spectatord_deps():
  http_archive(
          name = "curl",
          build_file = "@nflx_spectatord//third_party:curl.BUILD",
          strip_prefix = "curl-7.74.0",
          urls = [ "https://curl.haxx.se/download/curl-7.74.0.tar.gz", ],
          sha256 = "e56b3921eeb7a2951959c02db0912b5fcd5fdba5aca071da819e1accf338bbd7"
          )

  http_archive(
          name = "com_github_c_ares_c_ares",
          build_file = "@nflx_spectatord//third_party/cares:cares.BUILD",
          strip_prefix = "c-ares-1.15.0",
          sha256 = "6cdb97871f2930530c97deb7cf5c8fa4be5a0b02c7cea6e7c7667672a39d6852",
          url = "https://github.com/c-ares/c-ares/releases/download/cares-1_15_0/c-ares-1.15.0.tar.gz",
          )

  http_archive(
          name = "boringssl",
          sha256 = "1188e29000013ed6517168600fc35a010d58c5d321846d6a6dfee74e4c788b45",
          strip_prefix = "boringssl-7f634429a04abc48e2eb041c81c5235816c96514",
          urls = ["https://github.com/google/boringssl/archive/7f634429a04abc48e2eb041c81c5235816c96514.tar.gz"],
          )

  http_archive(
          name = "net_zlib",
          build_file = "@nflx_spectatord//third_party:zlib.BUILD",
          sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
          strip_prefix = "zlib-1.2.11",
          urls = [
              "https://mirror.bazel.build/zlib.net/zlib-1.2.11.tar.gz",
              "https://zlib.net/zlib-1.2.11.tar.gz",
              ],
          )

  http_archive(
          name = "com_github_skarupke_flat_hash_map",
          build_file = "@nflx_spectatord//third_party:flat_hash_map.BUILD",
          sha256 = "513efb9c2f246b6df9fa16c5640618f09804b009e69c8f7bd18b3099a11203d5",
          strip_prefix = "flat_hash_map-2c4687431f978f02a3780e24b8b701d22aa32d9c",
          urls = ["https://github.com/skarupke/flat_hash_map/archive/2c4687431f978f02a3780e24b8b701d22aa32d9c.zip"],
          )

  http_archive(
          name = "com_github_tessil_hopscotch_map",
          build_file = "@nflx_spectatord//third_party:hopscotch_map.BUILD",
          strip_prefix = "hopscotch-map-2.3.0",
          urls = ["https://github.com/Tessil/hopscotch-map/archive/v2.3.0.tar.gz"],
          sha256 = "a59d65b552dc7682521989842418c92257147f5068152b5af50e917892ad9317"
          )

  # https://github.com/envoyproxy/envoy/blob/master/bazel/repository_locations.bzl.
  http_archive(
          name = "com_github_fmtlib_fmt",
          build_file = "@nflx_spectatord//third_party:fmtlib.BUILD",
          strip_prefix = "fmt-7.1.3",
          urls = ["https://github.com/fmtlib/fmt/releases/download/7.1.3/fmt-7.1.3.zip"],
          sha256 = "5d98c504d0205f912e22449ecdea776b78ce0bb096927334f80781e720084c9f"
          )

  # https://github.com/envoyproxy/envoy/blob/master/bazel/repository_locations.bzl.
  http_archive(
          name = "com_github_tencent_rapidjson",
          build_file = "@nflx_spectatord//third_party:rapidjson.BUILD",
          sha256 = "bf7ced29704a1e696fbccf2a2b4ea068e7774fa37f6d7dd4039d0787f8bed98e",
          strip_prefix = "rapidjson-1.1.0",
          urls = ["https://github.com/Tencent/rapidjson/archive/v1.1.0.tar.gz"],
          )

  # https://github.com/envoyproxy/envoy/blob/master/bazel/repository_locations.bzl.
  http_archive(
          name = "com_github_gabime_spdlog",
          build_file = "@nflx_spectatord//third_party:spdlog.BUILD",
          strip_prefix = "spdlog-1.8.0",
          sha256 = "1e68e9b40cf63bb022a4b18cdc1c9d88eb5d97e4fd64fa981950a9cacf57a4bf",
          urls = ["https://github.com/gabime/spdlog/archive/v1.8.0.tar.gz"],
          )

  # asio
  http_archive(
          name = "asio",
          build_file = "@nflx_spectatord//third_party:asio.BUILD",
          urls = ["https://github.com/chriskohlhoff/asio/archive/asio-1-18-1.zip"],
          strip_prefix = "asio-asio-1-18-1",
          sha256 = "01e4c72985af0e19378e763d513ddc0c61886bc14556a14dd4220eb18304c383"
          )

  # GoogleTest/GoogleMock framework.
  http_archive(
          name = "com_google_googletest",
          urls = ["https://github.com/google/googletest/archive/release-1.10.0.tar.gz"],
          strip_prefix = "googletest-release-1.10.0",
          sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
          )

  # Google benchmark.
  http_archive(
          name = "com_google_benchmark",
          urls = ["https://github.com/google/benchmark/archive/v1.5.1.tar.gz"],
          strip_prefix = "benchmark-1.5.1",
          sha256 = "23082937d1663a53b90cb5b61df4bcc312f6dee7018da78ba00dd6bd669dfef2"
          )

  http_archive(
          name = "com_google_absl",
          urls = ["https://github.com/abseil/abseil-cpp/archive/0f3bb466b868b523cf1dc9b2aaaed65c77b28862.zip"],
          strip_prefix = "abseil-cpp-0f3bb466b868b523cf1dc9b2aaaed65c77b28862",
          sha256 = "9929f3662141bbb9c6c28accf68dcab34218c5ee2d83e6365d9cb2594b3f3171"
          )

  http_archive(
          name = "com_google_tcmalloc",
          urls = ["https://github.com/google/tcmalloc/archive/b28df226c5e6dfbc488f82514244d0b800a92685.zip"],
          strip_prefix = "tcmalloc-b28df226c5e6dfbc488f82514244d0b800a92685",
          sha256 = "8568fb0aaf3961bf26cd3ae570580e9ed1ce419c607b80b497f4a54f2c7441fa",
          )

  http_archive(
          name = "com_github_cyan4973_xxhash",
          urls = [ "https://github.com/Cyan4973/xxHash/archive/v0.8.0.tar.gz"],
          strip_prefix = "xxHash-0.8.0",
          build_file = "@nflx_spectatord//third_party:xxhash.BUILD",
          sha256 = "7054c3ebd169c97b64a92d7b994ab63c70dd53a06974f1f630ab782c28db0f4f",
          )

  http_archive(
          name = "com_github_bombela_backward",
          urls = [ "https://github.com/bombela/backward-cpp/archive/1efdd145b5fa84f457fb6727677ce0bc9f2c7b5b.zip" ],
          strip_prefix = "backward-cpp-1efdd145b5fa84f457fb6727677ce0bc9f2c7b5b",
          build_file = "@nflx_spectatord//third_party:backward.BUILD",
          sha256 = "97ddc265cc42afadf870ccfa9b079382766eb9e46e8fc1994a61a92ff547c851",
          )

  # netflix internal config
  http_archive(
          name = "nflx_spectator_cfg",
          build_file = "@nflx_spectatord//third_party:spectator_cfg.BUILD",
          urls = ["https://stash.corp.netflix.com/rest/api/latest/projects/CLDMTA/repos/netflix-spectator-cppconf/archive?at=e2fa37ea6fed338a3fe7b12c7701ec086d4c5713&format=zip"],
          sha256 = "08be5c03f7f90c7c8727a2ca395048b9651ca48620bd978db530414ff7b7d666",
          type = "zip",
      )

  # C++ rules for Bazel.
  http_archive(
          name = "rules_cc",
          urls = ["https://github.com/bazelbuild/rules_cc/archive/02becfef8bc97bda4f9bb64e153f1b0671aec4ba.zip"],
          strip_prefix = "rules_cc-02becfef8bc97bda4f9bb64e153f1b0671aec4ba",
          sha256 = "fa42eade3cad9190c2a6286a6213f07f1a83d26d9f082d56f526d014c6ea7444",
          )
