# Spectatord - A High Performance Metrics Daemon

[![Build Status](https://travis-ci.com/Netflix-Skunkworks/spectatord.svg?branch=master)](https://travis-ci.com/Netflix-Skunkworks/spectatord)

> :warning: Experimental

## Description

This project provides a high performance daemon that listens for updates to
meters like counters, timers, or gauges, sending aggregates periodically to an
atlas-aggregator.

## Endpoints

By default the daemon will listen on UDP port 1234
and on the Unix domain socket `/run/spectatord/spectatord.unix`

## Examples

```
$ echo "c:server.numRequests,id=failed:1" | nc -u -w0 localhost 1234
$ echo "t:server.requestLatency:0.042" | nc -u -w0 localhost 1234
$ echo "d:server.responseSizes:1024" | nc -w0 -uU /run/spectatord/spectatord.unix
$ echo "g:someGauge:60" | nc -w0 -uU /run/spectatord/spectatord.unix
$ echo "X,1543160297100:monotonic.Source:42" | nc -w0 -uU /run/spectatord/spectatord.unix
$ echo "X,1543160298100:monotonic.Source:43" | nc -w0 -uU /run/spectatord/spectatord.unix
```

## Format

The message sent to the server has the following format:

```
metric-type:name,tags:value@timestamp
```

where `tags` and `@timestamp` are optional.

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

## Some performance numbers

A key goal of this project is to have some good performance. This means we need to be able to 
use few resources for the common use cases where the number of metric updates is relatively small 
(fewer than 10k requests per second), and being able to scale to handle hundreds of thousands
of updates per second when that is required.

Using Unix domain sockets we can handle close to 1M metric updates per second,
assuming the client batches the updates and sends a few at a time. Sending
every single metric update requires a lot of context switching but is something that
works well for the majority of our use cases. This simplicity means the user does not have
to maintain any local state.

```
Transport          Batch Size    First 10M          Second 10M
Unix Dgram         1             22.98s (435k rps)  20.58s (486k rps)
Unix Dgram         8             11.46s (873k rps)   9.89s (1011k rps)
Unix Dgram         32            10.38s (963k rps)   8.49s (1178k rps)
```

The UDP transport is particularly sensitive the max receive buffer size (16MB on our systems). 

Our tests indicated that sending 430k rps to our UDP port did not drop packets but if there is a need for higher
throughput, then tweaking `/proc/sys/net/unix/max_dgram_qlen` is recommended.

