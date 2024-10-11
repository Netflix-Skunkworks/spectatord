import os
import shutil
import subprocess
from dataclasses import dataclass

from conans import ConanFile
from conans.tools import download, unzip, check_sha256


@dataclass
class NflxConfig:
    internal: str = os.getenv("NFLX_INTERNAL")
    source_host: str = os.getenv("NFLX_SOURCE_HOST")
    ssl_cert: str = os.getenv("NFLX_SSL_CERT")
    ssl_key: str = os.getenv("NFLX_SSL_KEY")

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
        "protobuf/3.21.12",
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
        self.options["libcurl"].with_ssl = "openssl"
        self.options["poco"].enable_data_mysql = False
        self.options["poco"].enable_data_postgresql = False
        self.options["poco"].enable_data_sqlite = False
        self.options["poco"].enable_jwt = False
        self.options["poco"].enable_mongodb = False
        self.options["poco"].enable_redis = False
        self.options["poco"].enable_activerecord = False

    @staticmethod
    def maybe_remove_dir(path: str):
        if os.path.isdir(path):
            shutil.rmtree(path)

    @staticmethod
    def maybe_remove_file(path: str):
        if os.path.isfile(path):
            os.unlink(path)

    @staticmethod
    def download(nflx_cfg: NflxConfig, repo: str, commit: str, zip_name: str) -> None:
        subprocess.run([
            "curl", "-s", "-k", "-L",
            "--cert", nflx_cfg.ssl_cert,
            "--key", nflx_cfg.ssl_key,
            "-H", "Accept: application/vnd.github+json",
            "-H", "X-GitHub-Api-Version: 2022-11-28",
            "-o", zip_name,
            f"https://{nflx_cfg.source_host}/api/v3/repos/{repo}/zipball/{commit}"
        ], check=True)

    def get_flat_hash_map(self):
        repo = "skarupke/flat_hash_map"
        commit = "2c4687431f978f02a3780e24b8b701d22aa32d9c"
        zip_name = repo.replace("skarupke/", "") + f"-{commit}.zip"

        self.maybe_remove_file(zip_name)
        download(f"https://github.com/{repo}/archive/{commit}.zip", zip_name)
        check_sha256(zip_name, "513efb9c2f246b6df9fa16c5640618f09804b009e69c8f7bd18b3099a11203d5")

        dir_name = "ska"
        self.maybe_remove_dir(dir_name)
        unzip(zip_name, destination=dir_name, strip_root=True)

        os.unlink(zip_name)

    def get_netflix_spectator_cppconf(self, nflx_cfg: NflxConfig) -> None:
        repo = "corp/cldmta-netflix-spectator-cppconf"
        commit = "190785a2205e96c71646a4f3dd6b8f5154e9a9ba"
        zip_name = repo.replace("corp/", "") + f"-{commit}.zip"

        self.maybe_remove_file(zip_name)
        self.download(nflx_cfg, repo, commit, zip_name)
        check_sha256(zip_name, "50d8641f2a38d4682c33fbb4a073f9e6679589e635ced09caadc54a7f55c26a5")

        dir_name = repo.replace("corp/", "")
        self.maybe_remove_dir(dir_name)
        unzip(zip_name, destination=dir_name, strip_root=True)
        self.maybe_remove_file("spectator/netflix_config.cc")
        shutil.move(f"{dir_name}/netflix_config.cc", "spectator")

        os.unlink(zip_name)
        shutil.rmtree(dir_name)

    def get_spectatord_metatron(self, nflx_cfg: NflxConfig) -> None:
        repo = "corp/cldmta-spectatord-metatron"
        commit = "544fec6b794e46da0af174062a35567a9d462e5f"
        zip_name = repo.replace("corp/", "") + f"-{commit}.zip"

        self.maybe_remove_file(zip_name)
        self.download(nflx_cfg, repo, commit, zip_name)
        check_sha256(zip_name, "edb0aebd7b391f72242fae0ee08c7a0ae86170743dffc49166f2f3e8a7062185")

        dir_name = repo.replace("corp/", "")
        self.maybe_remove_dir(dir_name)
        unzip(zip_name, destination=dir_name, strip_root=True)
        self.maybe_remove_file("metatron/auth_context.proto")
        self.maybe_remove_file("metatron/metatron_config.cc")
        shutil.move(f"{dir_name}/metatron/auth_context.proto", "metatron")
        shutil.move(f"{dir_name}/metatron/metatron_config.cc", "metatron")

        os.unlink(zip_name)
        shutil.rmtree(dir_name)

    def source(self) -> None:
        self.get_flat_hash_map()

        nflx_cfg = NflxConfig()

        if nflx_cfg.internal == "ON":
            if nflx_cfg.source_host is None:
                raise ValueError("NFLX_SOURCE_HOST must be set when NFLX_INTERNAL is ON")
            if nflx_cfg.ssl_cert is None:
                raise ValueError("NFLX_SSL_CERT must be set when NFLX_INTERNAL is ON")
            if nflx_cfg.ssl_key is None:
                raise ValueError("NFLX_SSL_KEY must be set when NFLX_INTERNAL is ON")
        else:
            return

        self.get_netflix_spectator_cppconf(nflx_cfg)
        self.get_spectatord_metatron(nflx_cfg)
