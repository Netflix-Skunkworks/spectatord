# Spectatord - A High Performance Metrics Daemon

[![Build Status](https://travis-ci.com/Netflix-Skunkworks/spectatord.svg?branch=master)](https://travis-ci.com/Netflix-Skunkworks/spectatord)

> :warning: Experimental

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
$ echo "X,1543160297100:monotonic.Source:42" | nc -w0 -uU /run/spectatord/spectatord.unix
$ echo "X,1543160298100:monotonic.Source:43" | nc -w0 -uU /run/spectatord/spectatord.unix
$ echo "A:age.gauge:0" | nc -u -w0 localhost 1234
```

## Format

The message sent to the server has the following format:

```
metric-type:name,tags:value
```

where the `,tags` portion is optional.

Multiple lines might be send in the same packet separated by newlines (`\n`).

### Metric Types

* `c` Counter. The value represents the number of increments
* `d` Distribution Summary.
* `t` Timer. The value is expressed in seconds
* `g` Gauge. This will report the last value set. An optional TTL
             can follow the `g` preceded by a comma. Otherwise the value will have
             a TTL of 15 minutes. For example:
             ```
             g,5:gauge:42.0
             ```
             This will use a 5s TTL.
* `m` Max Gauge. This will report the maximum value to the aggregator
* `A` Age Gauge. Report the number of seconds since this event. 
                 A value in seconds since this epoch can be set or 0 to use the current time. 
                 For example: 
  ```
  A:time.sinceLastSuccess:1611081000
  ```
  To use an explicit time, or simply:
  ```
  A:time.sinceLastSuccess:0
  ```
  to set `now()` as the last success.
* `C` Monotonic Counters. To track monotonic sources. `spectatord`
will convert it to a delta once it receives a second sample.
* `D` Percentile Distribution Summary
* `T` Percentile Timer. The value is expressed in seconds
* `X` Monotonic Counters sampled at a given time. This is an experimental type
that can be used to track monotonic sources that were sampled in the recent 
past. The timestamp in milliseconds of when the reported value was sampled must 
be included after the `X` preceded by a comma.

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
