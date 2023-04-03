# Third-Party Software Components

Under this directory, third-party software/libraries developed independently
of xrdp are imported as git submodules or subtree.  Each third-party software
is subject to its own software license.

See [COPYING-THIRD-PARTY](third_party/COPYING-THIRD-PARTY) for full license
statement of all third-party components.

```
third_party
└── tomlc99 ······ TOML C library (MIT License)
```

## Developer's Note

In order to show all license terms in xrdp, follow the steps below after
adding new third-party libraries.

1. Install [adobe/bin2c](https://github.com/adobe/bin2c) to your development
   environment
2. Write all third-party license terms to `COPYING-THIRD-PARTY`
3. Run `make -f Makefile.copying` to generate `copying_third_party.h`
4. Commit generated `copying_third_party.h`
