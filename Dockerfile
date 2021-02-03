FROM BASEOS_IMAGE
MAINTAINER Netflix Observability Engineering

RUN sudo apt update
RUN sudo apt install -y software-properties-common
RUN sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
RUN curl https://bazel.build/bazel-release.pub.gpg | sudo apt-key add -
RUN echo 'deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8' | sudo tee /etc/apt/sources.list.d/bazel.list
RUN sudo apt update
RUN sudo apt -y install bazel-3.7.2 binutils-dev g++-10 libiberty-dev
RUN sudo ln -s /usr/bin/bazel-3.7.2 /usr/bin/bazel

VOLUME /storage
