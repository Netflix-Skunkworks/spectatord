#!/bin/bash

SPECTATORD_IMAGE="spectatord/builder:latest"
SPECTATORD_IMAGE_ID=$(docker images --quiet $SPECTATORD_IMAGE)

if [[ -z "$SPECTATORD_IMAGE_ID" ]]; then
  if [[ -z "$BASEOS_IMAGE" ]]; then
    echo "set BASEOS_IMAGE to a reasonable value, such as ubuntu:bionic" && exit 1
  fi

  sed -i -e "s,BASEOS_IMAGE,$BASEOS_IMAGE,g" Dockerfile
  docker build --tag $SPECTATORD_IMAGE . || exit 1
  git checkout Dockerfile
else
  echo "using image $SPECTATORD_IMAGE $SPECTATORD_IMAGE_ID"
fi

# option to start an interactive shell in the source directory
if [[ "$1" == "shell" ]]; then
  docker run --rm --interactive --tty --mount type=bind,source="$(pwd)",target=/src --workdir /src $SPECTATORD_IMAGE /bin/bash
  exit 0
fi

# recommend 8GB RAM allocation for docker desktop, to allow the test build with asan to succeed
cat >start-build <<EOF
export CC="gcc-10"
export CXX="g++-10"

echo "-- build tests with address sanitizer enabled"
bazel --output_user_root=.cache build --config=asan spectator_test spectatord_test

echo "-- run tests"
bazel-bin/spectator_test && bazel-bin/spectatord_test

echo "-- build optimized daemon"
bazel --output_user_root=.cache build --compilation_mode=opt spectatord_main

echo "-- check shared library dependencies"
ldd bazel-bin/spectatord_main || true

echo "-- copy binary to local filesystem"
rm -f spectatord
cp -p bazel-bin/spectatord_main spectatord
EOF

chmod 755 start-build

docker run --rm --tty --mount type=bind,source="$(pwd)",target=/src $SPECTATORD_IMAGE /bin/bash -c "cd src && ./start-build"

rm start-build

# adjust symlinks to point to the local .cache directory
for link in bazel-*; do
  ln -nsf "$(readlink "$link" |sed -e "s:^/src/::g")" "$link"
done
