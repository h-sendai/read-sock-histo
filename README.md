# read-sock-histo

read()で読んだバイト数のヒストグラムデータを作る。

ヒストグラムデータを作るのにGNU Scientific Library (gsl)を使ってる。
入っていなければCentOS:
```
root# yum install gsl-devel
```
Debian:
```
root# apt install libgsl-dev
```
でセットしておく。

## 使い方

```
./read-sock-histo [-b bufsize] [-t run_sec] remote_host:port x_min x_max n_bin

```

- -b bufsize: デフォルト 2MB
- -t run_sec: 走らせる秒数。デフォルト10秒。

ヒストグラムデータはstdoutに、ランサマリはstderrにでる。

```
./read-sock-histo remote_host:port 0 128k 128 > histo.data 2>run.log
```
