# SpectatorD - A High Performance Metrics Daemon

[![Snapshot](https://github.com/Netflix-Skunkworks/spectatord/actions/workflows/snapshot.yml/badge.svg)](https://github.com/Netflix-Skunkworks/spectatord/actions/workflows/snapshot.yml)

## Description

This project provides a high performance daemon that listens for updates to
meters like counters, timers, or gauges, sending aggregates periodically to an
atlas-aggregator.

## Endpoints

By default the daemon will listen on the following endpoints:

* UDP port = 1234 *(~430K reqs/sec with 16MB buffers)*
* Unix domain socket = `/run/spectatord/spectatord.unix` *(~1M reqs/sec with batching)*

The choice of which endpoint to use is determined by your performance and access requirements;
the Unix domain socket offers higher performance, but requires filesystem access, which may not
be tenable under some container configurations. See [Performance Numbers](#performance-numbers)
for more details.

## Examples

```
$ echo "c:server.numRequests,id=failed:1" | nc -u -w0 localhost 1234
$ echo "t:server.requestLatency:0.042" | nc -u -w0 localhost 1234
$ echo "d:server.responseSizes:1024" | nc -w0 -uU /run/spectatord/spectatord.unix
$ echo "g:someGauge:60" | nc -w0 -uU /run/spectatord/spectatord.unix
$ echo "g,300:anotherGauge:60" | nc -w0 -uU /run/spectatord/spectatord.unix
$ echo "X,1543160297100:monotonic.Source:42" | nc -w0 -uU /run/spectatord/spectatord.unix
$ echo "X,1543160298100:monotonic.Source:43" | nc -w0 -uU /run/spectatord/spectatord.unix
$ echo "A:age.gauge:0" | nc -u -w0 localhost 1234
```

> :warning: In container environments, the `-w0` option may not work and `-w1` should be
used instead.

## Format

The message sent to the server has the following format, where the `,options` and `,tags` portions
are optional:

```
meter-type,options:name,tags:value
```

Multiple lines may be sent in the same packet, separated by newlines (`\n`):

```
$ echo -e "t:server.requestLatency:0.042\nd:server.responseSizes:1024" | nc -u -w0 localhost 1234
```

### Meter Types

| Symbol | Meter Type | Description |
|--------|------------|-------------|
| `c` | Counter | The value is the number of increments that have occurred since the last time it was recorded. |
| `d` | Distribution Summary | The value tracks the distribution of events. It is similar to a Timer, but more general, because the size does not have to be a period of time. <br><br> For example, it can be used to measure the payload sizes of requests hitting a server or the number of records returned from a query. |
| `g` | Gauge | The value is a number that was sampled at a point in time. The default time-to-live (TTL) for gauges is 900 seconds (15 minutes) - they will continue reporting the last value set for this duration of time. <br><br> Optionally, the TTL may be specified in seconds, with a minimum TTL of 5 seconds. For example, `g,120:gauge:42.0` spcifies a gauge with a 120 second (2 minute) TTL. |
| `m` | Max Gauge | The value is a number that was sampled at a point in time, but it is reported as a maximum gauge value to the backend. |
| `t` | Timer | The value is the number of seconds that have elapsed for an event. |
| `A` | Age Gauge | The value is the time in seconds since the epoch at which an event has successfully occurred, or `0` to use the current time in epoch seconds. After an Age Gauge has been set, it will continue reporting the number of seconds since the last time recorded, for as long as the spectatord process runs. The purpose of this meter type is to enable users to more easily implement the Time Since Last Success alerting pattern. <br><br> To set a specific time as the last success: `A:time.sinceLastSuccess:1611081000`. <br><br> To set `now()` as the last success: `A:time.sinceLastSuccess:0`. <br><br> By default, a maximum of `1000` Age Gauges are allowed per `spectatord` process, because there is no mechanism for cleaning them up. This value may be tuned with the `--age_gauge_limit` flag on the spectatord binary. |
| `C` | Monotonic Counter | The value is a monotonically increasing number. A minimum of two samples must be received in order for `spectatord` to calculate a delta value and report it to the backend. <br><br> A variety of networking metrics may be reported monotically and this meter type provides a convenient means of recording these values, at the expense of a slower time-to-first metric. |
| `D` | Percentile Distribution Summary | The value tracks the distribution of events, with percentile precision. It is similar to a Percentile Timer, but more general, because the size does not have to be a period of time. <br><br> For example, it can be used to measure the payload sizes of requests hitting a server or the number of records returned from a query. |
| `T` | Percentile Timer | The value is the number of seconds that have elapsed for an event, with percentile precision. <br><br> This meter type will track the data distribution by maintaining a set of Counters. The distribution can then be used on the server side to estimate percentiles, while still allowing for arbitrary slicing and dicing based on dimensions. <br><br> In order to maintain the data distribution, they have a higher storage cost, with a worst-case of up to 300X that of a standard Timer. Be diligent about any additional dimensions added to Percentile Timers and ensure that they have a small bounded cardinality. |
| `X` | Monotonic Counter with Millisecond Timestamps |  The value is a monotonically increasing number, sampled at a specified number of milliseconds since the epoch. A minimum of two samples must be received in order for `spectatord` to calculate a delta value and report it to the backend. <br><br> This is an experimental meter type that can be used to track monotonic sources that were sampled in the recent past, with the value normalized over the reported time period. <br><br> The timestamp in milliseconds since the epoch when the value was sampled must be included as a meter option: `X,1543160297100:monotonic.Source:42` |

### Name and Tags

The name must follow the atlas restrictions. For naming conventions see
[Spectator Naming Conventions](https://netflix.github.io/spectator/en/latest/intro/conventions/)

Tags are optional. They can be specified as comma separated key=value pairs. For example:

`fooIsTheName,some.tag=val1,some.otherTag=val2`

The restrictions on valid names and tag keys and values are:

#### Length

| Limit            | Min | Max |
|------------------|-----|-----|
| Length of `name` |   1 | 255 |
| Tag key length   |   2 |  60 |
| Tag value length |   1 | 120 |

#### Allowed Characters

Tag keys and values are only allowed to use characters in the set `-._A-Za-z0-9`. Others will
be converted to an `_` by the client.

### Value

A double value. The meaning of the value depends on the metric type.

## Metrics

See [METRICS](./METRICS.md) for a list of metrics published by this service.

## Performance Numbers

A key goal of this project is to deliver high performance. This means that we need to use few
resources for the common use case, where the number of metric updates is relatively small
(< 10k reqs/sec), and it also needs to be able to handle hundreds of thousands of updates per
second when required.

Using Unix domain sockets, we can handle close to 1M metric updates per second, assuming the client
batches the updates and sends a few at a time. Sending every single metric update requires a lot of
context switching, but is something that works well for the majority of our use cases. This
simplicity means the user does not have to maintain any local state.

```
Transport          Batch Size    First 10M          Second 10M
Unix Dgram         1             22.98s (435k rps)  20.58s (486k rps)
Unix Dgram         8             11.46s (873k rps)   9.89s (1011k rps)
Unix Dgram         32            10.38s (963k rps)   8.49s (1178k rps)
```

The UDP transport is particularly sensitive the max receive buffer size (16MB on our systems). 

Our tests indicate that sending 430K rps to the UDP port did not drop packets, but if there is a
need for higher throughput, then tweaking `/proc/sys/net/unix/max_dgram_qlen` is recommended.

## Performance Testing

* Start spectatord in debug mode (`--debug`) to send metrics to a dev stack of the Atlas aggregator,
which will perform validation and return the correct HTTP status codes for payloads, then drop the
metrics on the floor. Alternatively, you can also configure it to send metrics to `/dev/null`.
* Use the [`metrics_gen`](./tools/metrics_gen.cc) binary to generate and send a stream of metrics to a
running spectatord binary.
* Use the `perf-record` and `perf-report` Linux utilities to measure the performance of the running
binary.
* The [`udp_numbers.pl`](./tools/udp_numbers.pl) script is used to automate running `metrics_gen`
with different kernel settings for UDP sockets.

## Local Development

### Builds

* If you are running Docker Desktop, then allocate 8GB RAM to allow builds to succeed.
* Set the `BASEOS_IMAGE` environment variable to a reasonable value, such as `ubuntu:bionic`.
* Run the build: `./build.sh`
* Start an interactive shell in the source directory: `./build.sh shell`

### CLion

* Use JetBrains Toolbox to install version 2020.1.3 (latest is >= 2020.3.1).
* The older version of CLion is required to gain access to the [Bazel plugin] released by Google.
* You can build the Bazel plugin from source, to get the latest, which may fix more issues.
    ```
    git clone https://github.com/bazelbuild/intellij.git
    git checkout v2021.01.05
    bazel build //clwb:clwb_bazel_zip --define=ij_product=clion-beta
    bazel-bin/clwb/clwb_bazel.zip
    ```
* When loading a new project, use the `Import Bazel Project from the BUILD file` feature.
* If you need to remove the latest version of CLion and install an older one, disable JetBrains
settings sync and clear out all CLion locally cached data.
    ```
    rm -rf ~/Library/Application Support/CLion
    rm -rf ~/Library/Application Support/JetBrains/CLion*
    rm -rf $WORKSPACE/.idea
    ```

[Bazel plugin]: https://plugins.jetbrains.com/plugin/9554-bazel/versions
