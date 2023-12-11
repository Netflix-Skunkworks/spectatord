import os
import shutil

from conans import ConanFile
from conans.tools import download, unzip, check_sha256


class SpectatorDConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "abseil/20230125.3",
        "asio/1.28.1",
        "backward-cpp/1.6",
        "benchmark/1.8.3",
        "fmt/10.1.1",
        "gtest/1.14.0",
        "libcurl/8.4.0",
        "openssl/3.2.0",
        "poco/1.12.5p2",
        "rapidjson/cci.20230929",
        "spdlog/1.12.0",
        "tsl-hopscotch-map/2.3.1",
        "xxhash/0.8.2",
        "zlib/1.3"
    )
    generators = "cmake"
    default_options = {}

    def configure(self):
        self.options["libcurl"].with_c_ares = True
        self.options["poco"].enable_data_mysql = False
        self.options["poco"].enable_data_postgresql = False
        self.options["poco"].enable_data_sqlite = False
        self.options["poco"].enable_jwt = False
        self.options["poco"].enable_mongodb = False
        self.options["poco"].enable_redis = False
        self.options["poco"].enable_activerecord = False

    @staticmethod
    def get_flat_hash_map():
        dir_name = "ska"
        commit = "2c4687431f978f02a3780e24b8b701d22aa32d9c"
        if os.path.isdir(dir_name):
            shutil.rmtree(dir_name)
        zip_name = f"flat_hash_map-{commit}.zip"
        download(f"https://github.com/skarupke/flat_hash_map/archive/{commit}.zip", zip_name)
        check_sha256(zip_name, "513efb9c2f246b6df9fa16c5640618f09804b009e69c8f7bd18b3099a11203d5")
        unzip(zip_name)
        shutil.move(f"flat_hash_map-{commit}", dir_name)
        os.unlink(zip_name)

    @staticmethod
    def get_netflix_spectator_cppconf():
        if os.environ.get("NFLX_INTERNAL") != "ON":
            return
        dir_name = "netflix_spectator_cppconf"
        commit = "cc20c455e182e662f75d38de39d1ffb8f46fb94e"
        zip_name = f"nflx_spectator_cfg-{commit}.zip"
        download(f"https://stash.corp.netflix.com/rest/api/latest/projects/CLDMTA/repos/netflix-spectator-cppconf/archive?at={commit}&format=zip", zip_name)
        check_sha256(zip_name, "2a871e8182f371d73d5b6679c04c286ca7f185d87a0cfc535a126fcca3cc9863")
        unzip(zip_name, destination=dir_name)
        shutil.move(f"{dir_name}/netflix_config.cc", "spectator")
        os.unlink(zip_name)
        shutil.rmtree(dir_name)

    def source(self):
        self.get_flat_hash_map()
        self.get_netflix_spectator_cppconf()
