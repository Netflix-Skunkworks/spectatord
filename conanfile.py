import os
import shutil

from conans import ConanFile
from conans.tools import download, unzip, check_sha256


class SpectatorDConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "abseil/20210324.2",
        "asio/1.18.1",
        "backward-cpp/1.6",
        "benchmark/1.5.6",
        "c-ares/1.15.0",
        "fmt/7.1.3",
        "gtest/1.10.0",
        "libcurl/7.74.0",
        "rapidjson/1.1.0",
        "spdlog/1.8.0",
        "tsl-hopscotch-map/2.3.0",
        "xxhash/0.8.0",
        "zlib/1.2.11"
    )
    generators = "cmake"
    default_options = {}

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
        commit = "88e98808b2a5cbd60198faa79dd1eaee557ce71b"
        zip_name = f"nflx_spectator_cfg-{commit}.zip"
        download(f"https://stash.corp.netflix.com/rest/api/latest/projects/CLDMTA/repos/netflix-spectator-cppconf/archive?at={commit}&format=zip", zip_name)
        check_sha256(zip_name, "af4d6d9458bf367a1c6ec04e4dae6b15060446331f6f8edd66cf518f0f453dc1")
        unzip(zip_name, destination=dir_name)
        shutil.move(f"{dir_name}/netflix_config.cc", "spectator")
        os.unlink(zip_name)
        shutil.rmtree(dir_name)

    def source(self):
        self.get_flat_hash_map()
        self.get_netflix_spectator_cppconf()
