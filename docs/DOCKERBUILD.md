# Docker Build :hammer_and_wrench:

The `spectatord` project also supports a platform-agnostic build. The only prerequisite is the 
installation of `Docker`. Once `Docker` is installed, you can build the project by running the
following commands from the root directory of the project.

## Linux & Mac :penguin:

##### Warning:

- Do not prepend the command with `sudo` on Mac
- Start `Docker` before opening terminal on Mac 

```shell
sudo docker build -t spectatord-image -f Dockerfiles/Ubuntu.Dockerfile .
sudo docker run -it spectatord-image
./build.sh
```

## Windows

##### Warning:

- Start `Docker` before opening `Powershell`

```shell
docker build -t spectatord-image -f Dockerfiles/Ubuntu.Dockerfile .
docker run -it spectatord-image
apt-get install dos2unix
dos2unix build.sh
./build.sh
```