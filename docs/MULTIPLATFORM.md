
## Introduction

Notes on multiplatform support for spectatord.

## OSX

Default build tries to open the domain socket in a location that does not exist. Find alternative location for OSX,
or disable.

```shell
./cmake-build/bin/spectatord_main                      
libc++abi: terminating with uncaught exception of type std::__1::system_error: bind: No such file or directory
zsh: abort      ./cmake-build/bin/spectatord_main
```

When the domain socket is disabled, it runs, but: (1) /proc/net/udp cannot be opened, so no udp statistic metrics can
be published and (2) the receive buffer cannot be configured - find alternative.

```shell
./cmake-build/bin/spectatord_main --enable_socket=false
[2022-11-11 16:24:52.764] [spectator] [warning] Registry config has no uri. Ignoring start request
[2022-11-11 16:24:52.764] [spectatord] [info] Starting janitorial tasks
[2022-11-11 16:24:52.764] [spectator] [info] Will not expire meters
[2022-11-11 16:24:52.764] [spectatord] [info] Using receive buffer size = 16777216
[2022-11-11 16:24:52.764] [spectatord] [warning] Unable to open /proc/net/udp
[2022-11-11 16:24:52.765] [spectatord] [warning] Unable to set max receive buffer size: set_option: No buffer space available
[2022-11-11 16:24:52.765] [spectatord] [info] Starting spectatord server on port 1234
[2022-11-11 16:24:52.765] [spectatord] [info] statsd support is not enabled
[2022-11-11 16:24:52.765] [spectatord] [info] unix socket support is not enabled
```
