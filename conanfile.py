import os
import shutil
import subprocess
from dataclasses import dataclass
from typing import Optional

from conan import ConanFile
from conan.tools.files import download, unzip, check_sha256


@dataclass
class NflxConfig:
    internal: Optional[str] = os.getenv("NFLX_INTERNAL")
    source_host: Optional[str] = os.getenv("NFLX_SOURCE_HOST")
    ssl_cert: Optional[str] = os.getenv("NFLX_SSL_CERT")
    ssl_key: Optional[str] = os.getenv("NFLX_SSL_KEY")

class SpectatorDConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "abseil/20240116.2",
        "asio/1.32.0",
        "backward-cpp/1.6",
        "benchmark/1.8.3",
        "fmt/11.0.2",
        "gtest/1.15.0",
        "libcurl/8.10.1",
        "openssl/3.3.2",
        "poco/1.15.2",
        "protobuf/5.27.0",
        "rapidjson/cci.20230929",
        "spdlog/1.15.0",
        "tsl-hopscotch-map/2.3.1",
        "xxhash/0.8.2",
        "zlib/1.3.1",
    )
    tool_requires = (
        "protobuf/5.27.0",
    )
    generators = "CMakeDeps", "CMakeToolchain"

    def configure(self):
        # the default "dw" stack details on linux pulls in elfutils; backtrace_symbol has no extra dependencies
        self.options["backward-cpp"].stack_details = "backtrace_symbol"
        self.options["libcurl"].with_c_ares = True
        self.options["libcurl"].with_ssl = "openssl"
        self.options["poco"].enable_data = False
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
            "curl", "-s", "-L",
            "--connect-timeout", "5",
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
        download(self, f"https://github.com/{repo}/archive/{commit}.zip", zip_name)
        check_sha256(self, zip_name, "513efb9c2f246b6df9fa16c5640618f09804b009e69c8f7bd18b3099a11203d5")

        dir_name = "lib/ska"
        self.maybe_remove_dir(dir_name)
        unzip(self, zip_name, destination=dir_name, strip_root=True)

        os.unlink(zip_name)

    def get_netflix_spectator_cppconf(self, nflx_cfg: NflxConfig) -> None:
        repo = "corp/cldmta-netflix-spectator-cppconf"
        commit = "cef415c71068c4c4049076042922af36ed316249"
        zip_name = repo.replace("corp/", "") + f"-{commit}.zip"

        self.maybe_remove_file(zip_name)
        self.download(nflx_cfg, repo, commit, zip_name)
        check_sha256(self, zip_name, "9cf5bf8193a53ee3978a625bc5fb520807aff1240bdfc26da03a6a2936c43e15")

        dir_name = repo.replace("corp/", "")
        self.maybe_remove_dir(dir_name)
        unzip(self, zip_name, destination=dir_name, strip_root=True)
        self.maybe_remove_file("lib/spectator/registry/netflix_config.cc")
        shutil.move(f"{dir_name}/lib/src/netflix_config.cc", "lib/spectator/registry")

        os.unlink(zip_name)
        shutil.rmtree(dir_name)

    def get_spectatord_metatron(self, nflx_cfg: NflxConfig) -> None:
        repo = "corp/cldmta-spectatord-metatron"
        commit = "0159b93bd198d9846e92224acab012f6d355805a"
        zip_name = repo.replace("corp/", "") + f"-{commit}.zip"

        self.maybe_remove_file(zip_name)
        self.download(nflx_cfg, repo, commit, zip_name)
        check_sha256(self, zip_name, "efdadf4b254c547d6ff2dd07579ac97cced774d8d6645fd132d953881c0d6070")

        dir_name = repo.replace("corp/", "")
        self.maybe_remove_dir(dir_name)
        unzip(self, zip_name, destination=dir_name, strip_root=True)
        self.maybe_remove_file("lib/metatron/auth_context.proto")
        self.maybe_remove_file("lib/metatron/metatron_config.cc")
        shutil.move(f"{dir_name}/lib/metatron/auth_context.proto", "lib/metatron")
        shutil.move(f"{dir_name}/lib/metatron/metatron_config.cc", "lib/metatron")

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
