## Building

```
cmake -D CMAKE_C_COMPILER=gcc  . && cmake --build . && scp writer [writer host] && scp reader [reader host]
```

## On the writer host

`Usage: ./writer [aeron directory] [publication channel] [archive channel] [archive response channel]`

We run the writer, which continuously writes a message to the archive via a multicast publication.

```
./writer '/dev/shm/aeronmd-main' 'aeron:udp?endpoint=239.192.10.91:20123|interface=172.18.11.69/26|fc=min' 'aeron:udp?endpoint=172.18.150.109:8010' 'aeron:udp?endpoint=172.18.150.119:0'
```

## On the reader host

`Usage: ./reader [aeron directory] [subscription channel] [archive channel] [archive response channel] [replay_destination]`

We attempt to replay-merge, first by playing back the replay and then merging with the live publication.

```
./reader '/dev/shm/aeronmd-main' 'aeron:udp?endpoint=239.192.10.91:20123|interface=172.18.11.69/26|fc=min' 'aeron:udp?endpoint=172.18.150.109:8010' 'aeron:udp?endpoint=172.18.150.118:0' '172.18.150.118:0'
```

However, we see the following being logged and nothing more

```
Got data; will not log any further in this callback.
Live destination added
```
If we'd successfully merged, we'd also expect to see

```
Replay has caught up
```

Thanks!
