LibTorrent
========

Introduction
------------

High performance torrent library for multiple clients.

LibTorrent is a BitTorrent library written in C++ for *nix, with a focus on high performance and good code. The library differentiates itself from other implementations by transferring directly from file pages to the network stack.


To learn how to use libtorrent visit the [Wiki](https://github.com/rakshasa/libtorrent/wiki).

Related Projects
----------------

* [https://github.com/rakshasa/rbedit](https://github.com/rakshasa/rbedit): A dependency-free bencode editor.
* [https://github.com/rakshasa/rtorrent](https://github.com/rakshasa/rtorrent): high performance ncurses based torrent client



Donate to rTorrent development
------------------------------

* [Paypal](https://paypal.me/jarisundellno)
* [Patreon](https://www.patreon.com/rtorrent)
* [SubscribeStar](https://www.subscribestar.com/rtorrent)
* Bitcoin: 1MpmXm5AHtdBoDaLZstJw8nupJJaeKu8V8
* Ethereum: 0x9AB1e3C3d8a875e870f161b3e9287Db0E6DAfF78
* Litecoin: LdyaVR67LBnTf6mAT4QJnjSG2Zk67qxmfQ
* Cardano: addr1qytaslmqmk6dspltw06sp0zf83dh09u79j49ceh5y26zdcccgq4ph7nmx6kgmzeldauj43254ey97f3x4xw49d86aguqwfhlte


Help keep rTorrent development going by donating to its creator.

BUILD DEPENDENCIES
--------
Ubuntu, for example
```
sudo apt install make curl-libcurlpp-dev autoconf
```

BUILDING
--------

Jump into the github cloned directory

```
cd libtorrent
```

Create config files
```
autoreconf -ivf
```

Perform configuration
```
./configure
```

Compile, slowest option.
```
make
```

OR Optionally compile with multiple worker threads, optimal number of threads is number of cores -1 for fastest compile time. For example : a 4 core machine should compile with only 3 cores, an 8-core machine 7, etc.)  
```
make -j7
```

Install
```
make install
```

## Packaging status
[![status](https://repology.org/badge/version-for-repo/alpine_3_22/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/alpine_edge/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/alt_p11/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/arch/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/crux_38/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/cygwin/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/debian_13/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/debian_unstable/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/epel_9/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/fedora_42/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/freebsd/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/gentoo/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/gnuguix/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/homebrew/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/macports/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/nix_unstable/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/openbsd/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/opensuse_leap_15_6/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/opensuse_tumbleweed/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/solus/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/termux/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/ubuntu_22_04/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/ubuntu_24_04/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)  
[![status](https://repology.org/badge/version-for-repo/void_x86_64/libtorrent-rakshasa.svg)](https://repology.org/project/libtorrent-rakshasa/versions)
