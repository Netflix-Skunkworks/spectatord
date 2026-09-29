#!/bin/sh

VERSION=`git describe --tags --always || echo Unknown`

echo static constexpr const char* VERSION = \"$VERSION\"\;
