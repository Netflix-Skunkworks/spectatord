
# Conan Setup

```shell
./setup-venv.sh
source venv/bin/activate
```

* CLion > Preferences > Plugins > Marketplace > Conan > Install
* CLion > Preferences > Build, Execution, Deployment > Conan > Conan Executable > $PROJECT_HOME/venv/bin/conan
* CLion > Bottom Bar: Conan > Left Button: Match Profile > CMake Profile: Debug, Conan Profile: default

# Conan Search Packages and Inspect Metadata

```shell
conan search "*"
conan search poco
conan search poco --remote conancenter
conan inspect poco/1.12.2
```

# Conan Install Packages

```shell
# install requires packages locally
conan install . --build=missing --install-folder cmake-build

# install package sources locally
conan source .
```

# CMake Build

```shell
cd cmake-build
cmake .. 
cmake --build . -DCMAKE_BUILD_TYPE=Release
```
