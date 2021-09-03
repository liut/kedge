Kedge
===

A bittorrent engine base on libtorrent-rasterbar with RESTful API

JSON APIs
---

* `GET` `/sess` current session information, 200
* `GET` `/stats` statistices of session, 200
* `GET` `/torrents` show all torrents, 200
* `POST` `/torrents` new task with torrent file in body, 204 | 500
* `GET` `/torrent/{infohash}` show a torrent status, 200 | 404
* `GET` `/torrent/{infohash}/{act}` (files|peers), 200 | 404
* `HEAD` `/torrent/{infohash}`, 204 | 404
* `DELETE` `/torrent/{infohash}` remove a torrent, 204 | 404

**`infohash`: 40 bytes**

TODO
---
* split logs with alert types


## API Test

### add a torrent
```bash
curl -v -X POST --data-binary @debian-10.10.0-amd64-netinst.iso.torrent -H 'x-save-path: /tmp' http://localhost:16180/torrents
```

### show all torrents
```bash
curl http://localhost:16180/torrents | jq
```

### check a torrent exist
```bash
curl -v -I http://localhost:16180/torrent/5e13283d8dc83894fa899b4702f114dcab6b736e
```

### drop a torrent
```bash
curl -v -X DELETE http://localhost:16180/torrent/5e13283d8dc83894fa899b4702f114dcab6b736e
# or
curl -v -X DELETE http://localhost:16180/torrent/5e13283d8dc83894fa899b4702f114dcab6b736e/with_data
```

### show session stats
```bash
curl http://localhost:16180/stats | jq
```

## Dependencies
* [boost](https://www.boost.org/users/download/) >= 1.75
* [libtorrent-rasterbar](https://github.com/arvidn/libtorrent/releases) >= 1.2.14

### Components
* [Boost.Asio](https://github.com/boostorg/asio) net, io context, ...
* [Boost.Beast](https://github.com/boostorg/beast) http, websocket
* [Boost.JSON](https://github.com/boostorg/json) new JSON relative to PropertyTree!
* [Boost.ProgramOption](https://github.com/boostorg/program_options) config and options
* [Beast Router](https://github.com/shirayukikitsune/beast-router) A simple router(changed)

## Build

### macOS
```bash
sudo port install cmake clang-11 llvm-11
sudo port install zlib bzip2 openssl
```

### debian/ubuntu
```bash
sudo apt install cmake automake libtool pkg-config libgnutls28-dev libcurl4-gnutls-dev zlib1g-dev
sudo apt install clang-11 libc++-11-dev libclang-11-dev
```

### Install boost & libtorrent

```bash
echo 'using clang : 11.0 : : <cxxflags> -std=c++17 -O2 -no-pie -fPIC ;' >> ~/user-config.jam

export CPPFLAGS='-std=c++17 -no-pie -fPIC'

# download boost and uncompress it into ~/tmp
cd ~/tmp/boost_1_76_0/
./bootstrap.sh --prefix=/opt/boost --with-toolset=clang

./b2 -j12 --build-dir=build --prefix=/opt/boost --with=all cxxstd=17 toolset=clang link=static runtime-link=static variant=release threading=multi install
cd tools/build
./b2 -j12 --build-dir=build --prefix=/opt/boost --with=all cxxstd=17 toolset=clang variant=release install

# donwload libtorrent and uncompress it into ~/tmp
cd libtorrent

/opt/boost/bin/b2 -j12 --prefix=/opt/lt12 cxxstd=17 variant=release crypto=openssl link=static runtime-link=static install
```

### compile
```bash
rm -rf build && mkdir build && cd build
cmake ..
make -j6
```

