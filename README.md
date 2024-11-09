[![Build](https://github.com/Netflix-Skunkworks/spectatord/actions/workflows/build.yml/badge.svg)](https://github.com/Netflix-Skunkworks/spectatord/actions/workflows/build.yml)

# SpectatorD Introduction

SpectatorD is a high-performance telemetry daemon that listens for metrics specified by a
text-based protocol and publishes updates periodically to an [Atlas] aggregator service.
It consolidates the logic required to apply common tagging to all metrics received, maintain
metric lifetimes, and route metrics to the correct backend.

See the [Atlas Documentation] site for more details on SpectatorD.

[Atlas]: https://github.com/Netflix/atlas
[Atlas Documentation]: https://netflix.github.io/atlas-docs/spectator/agent/usage/

## Performance Testing

* Start `spectatord` in debug mode (`--debug`) to send metrics to a dev stack of the Atlas aggregator,
which will perform validation and return the correct HTTP status codes for payloads, then drop the
metrics on the floor. Alternatively, you can also configure it to send metrics to `/dev/null`.
* Use the [`metrics_gen`](./tools/metrics_gen.cc) binary to generate and send a stream of metrics to
a running spectatord binary.
* Use the `perf-record` and `perf-report` Linux utilities to measure the performance of the running
binary.
* The [`udp_numbers.pl`](./tools/udp_numbers.pl) script is used to automate running `metrics_gen`
with different kernel settings for UDP sockets.

## Local Development

```shell
# setup python venv and activate, to gain access to conan cli
./setup-venv.sh
source venv/bin/activate

# link clion default build directory to our build directory
ln -s cmake-build cmake-build-debug

./build.sh  # [clean|clean --force|skiptest]

# run the binary locally
./cmake-build/bin/spectatord_main --enable_external --no_common_tags

# test publish
echo "c:server.numRequests,nf.app=spectatord_$USER,id=failed:1" | nc -u -w0 localhost 1234
echo "c:server.numRequests,nf.app=spectatord_$USER,id=success:1" | nc -u -w0 localhost 1234

# parse errors
echo ":server.numRequests,nf.app=spectatord_$USER,id=failed:1" | nc -u -w0 localhost 1234
echo "c:server.numRequests,nf.app=spectatord_$USER,id=failed" | nc -u -w0 localhost 1234
echo "c::1" | nc -u -w0 localhost 1234
```

* CLion > Preferences > Plugins > Marketplace > Conan > Install
* CLion > Preferences > Build, Execution, Deploy > Conan > Conan Executable: $PROJECT_HOME/venv/bin/conan

## Errata

When building locally on MacOS, you may see the following errors when running binaries compiled with ASAN:

```
malloc: nano zone abandoned due to inability to reserve vm space.
```

This warning can be turned off with `export MallocNanoZone=0` ([link](https://stackoverflow.com/a/70209891/1382138)).
