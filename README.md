# alcor-chain

alcor-chain is [Alcor](https://alcor.exchange)'s proposal for an [Antelope](https://github.com/AntelopeIO) chain built
for on-chain trading: our variant of the node, with the features we think a trading chain needs. It is a fork of
[Spring](https://github.com/AntelopeIO/spring) v1.2.2 (Savanna consensus) and keeps its tools, APIs and light-client
finality proofs. It is open for the Antelope community to run, review and take from.

**Status: pre-release, for testnets.** Not for production.

## Testnet

| | |
|---|---|
| Faucet (account + test HORN) | https://acc.alcor.exchange/faucet |
| API | https://api.acc.alcor.exchange |
| History API (Hyperion) | https://history.acc.alcor.exchange ([docs](https://history.acc.alcor.exchange/v2/docs)) |
| Explorer | https://explorer.alcor.exchange/acc |
| P2P | `p2p.acc.alcor.exchange:9876`, `p2p2.acc.alcor.exchange:9876` (block producers: send us your node's IP, the ports are allowlisted; peer with both) |
| Community nodes | EOSUSA: open P2P `test.acc.p2p.eosusa.io:19878`, API https://test.acc.eosusa.io, snapshots https://snapshots.eosusa.io/snaps/acctestnet |
| | Saltant (Moscow): open P2P `185.185.58.170:9878` |
| Chain ID | `a23b0ec475262075fa3b99a38614e243c00275f777fce93e2d70a4f8d367e3fb` |
| Genesis | [`testnet/genesis.json`](testnet/genesis.json) |
| System token | `HORN` (4 decimals), contract `eosio.token` |
| Block time | 200 ms, 30 blocks per producer round, Savanna finality |
| Release | [`v1.2.2-alcor-testnet.3`](https://github.com/alcorexchange/alcor-chain/releases/tag/v1.2.2-alcor-testnet.3) (see [Versions](#versions)) |
| Chat | https://t.me/antelopecommunitychain |

Wallets (Anchor, WharfKit and others): add a custom network with the chain ID and API above. MetaMask and Phantom
keys work too: the faucet makes an account for them (buttons "Use MetaMask" / "Use Phantom"). Test HORN has no value.

### Versions

All `v1.2.2-alcor-testnet.*` tags run the same testnet (one chain ID, no consensus change between them). Use the
latest; a node on an older one keeps syncing.

| Tag | What changed |
|---|---|
| `testnet.3` | `.deb` in the release (Ubuntu 22.04); `cmake` installs to `/usr` by default; the version string matches the tag (`testnet.2` binaries still report `testnet.1`) |
| `testnet.2` | `cleos --sign-with` takes a bare `PUB_EM_...` / `PUB_ED_...` key |
| `testnet.1` | the testnet launch |

`nodeos --full-version` prints the version and the commit.

### Run a node

**Ubuntu 22.04: install the release package.**

```bash
V=1.2.2-alcor-testnet.3
wget https://github.com/alcorexchange/alcor-chain/releases/download/v$V/antelope-spring_$V-ubuntu22.04_amd64.deb \
     https://github.com/alcorexchange/alcor-chain/releases/download/v$V/SHA256SUMS
sha256sum -c --ignore-missing SHA256SUMS
sudo apt install ./antelope-spring_$V-ubuntu22.04_amd64.deb
```

**Any Linux with Docker: the pinned reproducible build.** The host's compilers do not matter:

```bash
git clone --recursive https://github.com/alcorexchange/alcor-chain.git
cd alcor-chain && git checkout v1.2.2-alcor-testnet.3 && git submodule update --init --recursive
DOCKER_BUILDKIT=1 docker build --build-arg SPRING_BUILD_JOBS=8 -f tools/reproducible.Dockerfile -o . .
sudo apt install ./antelope-spring_*.deb
```

**Native build.** About 4 GB of RAM per compile job, lower `-j` on a small box. The `.deb` installs to `/usr/bin`.

Ubuntu 22.04:

```bash
sudo apt install -y build-essential cmake git file python3 python3-numpy \
     libcurl4-openssl-dev libgmp-dev zlib1g-dev llvm-11-dev
git clone --recursive https://github.com/alcorexchange/alcor-chain.git
cd alcor-chain && git checkout v1.2.2-alcor-testnet.3 && git submodule update --init --recursive
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/lib/llvm-11 ..
make -j "$(nproc)" package
sudo apt install ./antelope-spring_*.deb
```

Ubuntu 24.04 has no LLVM 11, so build without EOS VM OC, the optional optimizing contract compiler that needs it.
The node then runs contracts on eos-vm-jit, with the same results (not yet tried by us: tell us in the chat if it
breaks):

```bash
sudo apt install -y build-essential cmake git file python3 libcurl4-openssl-dev libgmp-dev zlib1g-dev
# clone and check out as above, then:
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_OC=OFF ..
make -j "$(nproc)" package
sudo apt install ./antelope-spring_*.deb
```

If `cmake` stops with "GMP shared library not found" or "Could NOT find ZLIB", a package from the list is missing:
install it and run `cmake` again in an empty `build` directory.

Start it with the testnet genesis and peer with us and with the open community peers. `--genesis-json` is a command
line option only: nodeos does not read it from `config.ini`.

```bash
nodeos --genesis-json testnet/genesis.json \
  --plugin eosio::chain_api_plugin --plugin eosio::http_plugin \
  --p2p-peer-address p2p.acc.alcor.exchange:9876 \
  --p2p-peer-address p2p2.acc.alcor.exchange:9876 \
  --p2p-peer-address test.acc.p2p.eosusa.io:19878 \
  --p2p-peer-address 185.185.58.170:9878 \
  --chain-state-db-size-mb 16384 \
  --max-reversible-blocks 36000
```

`--max-reversible-blocks 36000` is 2 hours at 200 ms; the default 3600 is only 12 minutes.

To skip the sync from genesis, start from a snapshot instead (EOSUSA's, above): unpack it and replace
`--genesis-json testnet/genesis.json` with `--snapshot <file>.bin` on the first start, with an empty data directory.

From some networks (we saw it from Russia) connections to our P2P ports, which are on Hetzner, stall after the first
~16 KB: the node connects but never syncs. Peer through the community nodes then (`185.185.58.170:9878` is in Moscow).

### Become a block producer

1. Generate fresh testnet keys on your own machine (never reuse mainnet keys):
   - account owner and active keys: `cleos create key --to-console`
   - a block signing key: `cleos create key --to-console` (K1; EM and ED keys cannot sign blocks)
   - a BLS finalizer key with its proof of possession: `spring-util bls create key --to-console`
2. Send us in the chat your account name, owner and active public keys and your node's public IP. We create the
   account with test HORN and allowlist your IP for p2p.
3. Run a node in sync with the testnet (above), with your producer settings:

   ```ini
   producer-name = <your account>
   signature-provider = <signing public key>=KEY:<signing private key>
   signature-provider = <BLS public key>=KEY:<BLS private key>
   ```

4. Register as a producer and register your finalizer key:

   ```bash
   cleos -u https://api.acc.alcor.exchange system regproducer <account> <signing public key> <url> <location>
   cleos -u https://api.acc.alcor.exchange push action eosio regfinkey \
     '["<account>","<BLS public key>","<BLS proof of possession>"]' -p <account>@active
   ```

   `<location>` orders the schedule: producers go west to east, so each one hands over to a neighbor and blocks
   travel the shortest way. Set it to `round((longitude + 180) * 100)` of your node, 0 to 36000. For example,
   Falkenstein (12.4° E) is 19237, New York (74.0° W) is 10600, Tokyo (139.7° E) is 31970.

5. Get into the schedule. Producers are elected by votes (the schedule is the top 21 by votes, updated every minute),
   and you can vote for yourself with staked HORN; while there are fewer than 21 producers, any vote gets you in:

   ```bash
   cleos -u https://api.acc.alcor.exchange system delegatebw <account> <account> "40.0000 HORN" "40.0000 HORN"
   cleos -u https://api.acc.alcor.exchange system voteproducer prods <account> <account>
   ```

   Tell us in the chat once you produce. You produce 30 blocks (6 s) per round.

The finalizer policy is still set by hand: we add new finalizers once their nodes have run stable for a while, so a
node that goes down costs its own slots, never the chain's finality.

### Related repositories

- [system-contracts](https://github.com/alcorexchange/system-contracts): eosio.system with `setprioacts`, 200 ms block counts and the schedule ordered by location
- [cdt](https://github.com/alcorexchange/cdt): CDT with the block interval and epoch as build options; contracts for this chain need it
- [hyperion](https://github.com/alcorexchange/hyperion), [node-abieos](https://github.com/alcorexchange/node-abieos),
  [abieos](https://github.com/alcorexchange/abieos): history API and ABI serializer with EM/ED keys and 200 ms blocks


## What differs from Spring 1.2.2

- **200 ms blocks.** Block interval 200 ms, block timestamp epoch 2025-01-01 (genesis must not be earlier).
  Contracts that use block time need the matching constants in CDT.
- **30 blocks per producer round** (Spring: 12). A producer hands over every 6 s instead of every 2.4 s, so the
  pause at each handover (the next producer waits for the last block of the previous one) costs less time.
- **EM and ED account keys, from genesis.** EM = secp256k1 as in Ethereum wallets (MetaMask, Rabby), ED = ed25519 as
  in Solana wallets (Phantom). `PUB_EM_04<hex>` / `PUB_ED_<base58>`, signatures `SIG_EM_0x<r s v>` /
  `SIG_ED_<base58 of key and signature>`. The wallet signs a readable text, not a bare hash:
  `Alcor Chain transaction` / `Sign only in an app you trust` / the transaction digest in hex, as an EIP-191 personal
  message (EM) or an ed25519 message (ED); the node rebuilds exactly this text. One encoding per EM signature
  (v 27 or 28, low s). EM and ED keys work in account permissions only, never as block signing keys.
  `cleos create key --em` / `--ed`.
- **`ed25519_verify` host function, from genesis:** `ed25519_verify(msg, sig, pub)` returns 0 for a valid signature,
  -1 otherwise; messages up to 64 KiB. Ed25519 is libsodium 1.0.22, built in.
- **Priority actions** (cancels first in a block). The system contract keeps a list of (contract, action) pairs, set
  by the block producers' multisig (`eosio::setprioacts`, at most 64). A transaction made only of listed actions is a
  priority transaction. While the list is not empty, the producer does not apply transactions as they come: near the
  end of its slot it applies them in one batch, priority transactions first, then the rest in the order they came.
  So a market maker's cancel goes before every taker in the same block, whoever was faster, and nothing is applied
  twice. It is a block producer policy, not a consensus rule: every block must have its priority transactions
  first, and anyone can check that. Market makers should still send cancels to the current and next producer, use
  `cloid` and `expires_at`.
- **Security fixes:**
  - BLS host functions (`bls_g1_weighted_sum`, `bls_g2_weighted_sum`, `bls_pairing`) check span sizes in 64 bits. In 32
    bits a large count wrapped, and the node tried to allocate tens of GB.
  - A failed transaction is charged to its first authorizer (subjective billing, failure limit) only after its
    authorizations were verified. Before, anyone could charge any account.
  - P2P messages are decoded within their frame.
  - State history limits unanswered status requests per session.
  - The Savanna vote aggregator no longer gives up on a strong QC that is still reachable.
  - SHA-3 works on big-endian hosts.
- **Fixes from open Spring pull requests:** the strong-QC lockout fix (#1888), the replay, sync and test fixes
  (#1873–#1881), and the pinned package and tarball hash checks.

The chain's system contracts (with `setprioacts`) are in a separate repository.

---

## Spring

1. [Branches](#branches)
2. [Supported Operating Systems](#supported-operating-systems)
3. [Binary Installation](#binary-installation)
4. [Build and Install from Source](#build-and-install-from-source)
5. [Bash Autocomplete](#bash-autocomplete)

Spring is a C++ implementation of the [Antelope](https://github.com/AntelopeIO) protocol with support for Savanna consensus. It contains blockchain node software and supporting tools for developers and node operators.

## Branches
The `main` branch is the development branch; do not use it for production. Refer to the [release page](https://github.com/AntelopeIO/spring/releases) for current information on releases, pre-releases, and obsolete releases, as well as the corresponding tags for those releases.

## Supported Operating Systems
We currently support the following operating systems.
- Ubuntu 22.04 Jammy
- Ubuntu 20.04 Focal

Other Unix derivatives such as macOS are tended to on a best-effort basis and may not be full featured. If you aren't using Ubuntu, please visit the "[Build Unsupported OS](./docs/00_install/01_build-from-source/00_build-unsupported-os.md)" page to explore your options.

If you are running an unsupported Ubuntu derivative, such as Linux Mint, you can find the version of Ubuntu your distribution was based on by using this command:
```bash
cat /etc/upstream-release/lsb-release
```
Your best bet is to follow the instructions for your Ubuntu base, but we make no guarantees.

## Binary Installation
This is the fastest way to get started. From the [latest release](https://github.com/AntelopeIO/spring/releases/latest) page, download a binary for one of our [supported operating systems](#supported-operating-systems), or visit the [release tags](https://github.com/AntelopeIO/spring/releases) page to download a binary for a specific version of Spring.

Once you have a `*.deb` file downloaded for your version of Ubuntu, you can install it as follows:
```bash
sudo apt-get update
sudo apt-get install -y ~/Downloads/spring*.deb
```
Your download path may vary. If you are in an Ubuntu docker container, omit `sudo` because you run as `root` by default.

Finally, verify Spring was installed correctly:
```bash
nodeos --full-version
```
You should see a [semantic version](https://semver.org) string followed by a `git` commit hash with no errors. For example:
```
v1.0.1-9026a03c09c9b4f93edca696b5eef259f0ab96b3
```

## Build and Install from Source
You can also build and install Spring from source.

### Prerequisites
You will need to build on a [supported operating system](#supported-operating-systems).

Requirements to build:
- C++20 compiler and standard library
- CMake 3.16+
- LLVM 7 - 11 - for Linux only
  - newer versions do not work
- libcurl 7.40.0+
- git
- GMP
- Python 3
- python3-numpy
- zlib

### Step 1 - Clone
If you don't have the Spring repo cloned to your computer yet, [open a terminal](https://itsfoss.com/open-terminal-ubuntu) and navigate to the folder where you want to clone the Spring repository:
```bash
cd ~/Downloads
```
Clone Spring using either HTTPS...
```bash
git clone --recursive https://github.com/AntelopeIO/spring.git
```
...or SSH:
```bash
git clone --recursive git@github.com:AntelopeIO/spring.git
```

> ℹ️ **HTTPS vs. SSH Clone** ℹ️  
Both an HTTPS or SSH git clone will yield the same result - a folder named `spring` containing our source code. It doesn't matter which type you use.

Navigate into that folder:
```bash
cd spring
```

### Step 2 - Checkout Release Tag or Branch
Choose which [release](https://github.com/AntelopeIO/spring/releases) or [branch](#branches) you would like to build, then check it out. If you are not sure, use the [latest release](https://github.com/AntelopeIO/spring/releases/latest). For example, if you want to build release 1.0.1 then you would check it out using its tag, `v1.0.1`. In the example below, replace `v0.0.0` with your selected release tag accordingly:
```bash
git fetch --all --tags
git checkout v0.0.0
```

Once you are on the branch or release tag you want to build, make sure everything is up-to-date:
```bash
git pull
git submodule update --init --recursive
```

### Step 3 - Build
Select build instructions below for a [pinned build](#pinned-build) (preferred) or an [unpinned build](#unpinned-build).

> ℹ️ **Pinned vs. Unpinned Build** ℹ️  
We have two types of builds for Spring: "pinned" and "unpinned." A pinned build is a reproducible build with the build environment and dependency versions fixed by the development team. In contrast, unpinned builds use the dependency versions provided by the build platform. Unpinned builds tend to be quicker because the pinned build environment must be built from scratch. Pinned builds, in addition to being reproducible, ensure the compiler remains the same between builds of different Spring major versions. Spring requires the compiler version to remain the same, otherwise its state might need to be recovered from a portable snapshot or the chain needs to be replayed.

> ⚠️ **A Warning On Parallel Compilation Jobs (`-j` flag)** ⚠️  
When building C/C++ software, often the build is performed in parallel via a command such as `make -j "$(nproc)"` which uses all available CPU threads. However, be aware that some compilation units (`*.cpp` files) in Spring will consume nearly 4GB of memory. Failures due to memory exhaustion will typically, but not always, manifest as compiler crashes. Using all available CPU threads may also prevent you from doing other things on your computer during compilation. For these reasons, consider reducing this value.

> 🐋 **Docker and `sudo`** 🐋  
If you are in an Ubuntu docker container, omit `sudo` from all commands because you run as `root` by default. Most other docker containers also exclude `sudo`, especially Debian-family containers. If your shell prompt is a hash tag (`#`), omit `sudo`.

#### Pinned Reproducible Build
The pinned reproducible build requires Docker. Make sure you are in the root of the `spring` repo and then run
```bash
DOCKER_BUILDKIT=1 docker build -f tools/reproducible.Dockerfile -o . .
```
This command will take a substantial amount of time because a toolchain is built from scratch. Upon completion, the current directory will contain a built `.deb` and `.tar.gz` (you can change the `-o .` argument to place the output in a different directory). If needing to reduce the number of parallel jobs as warned above, run the command as,
```bash
DOCKER_BUILDKIT=1 docker build --build-arg SPRING_BUILD_JOBS=4 -f tools/reproducible.Dockerfile -o . .
```

#### Unpinned Build
The following instructions are valid for this branch. Other release branches may have different requirements, so ensure you follow the directions in the branch or release you intend to build. If you are in an Ubuntu docker container, omit `sudo` because you run as `root` by default.

Install dependencies:
```bash
sudo apt-get update
sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        libcurl4-openssl-dev \
        libgmp-dev \
        llvm-11-dev \
        python3-numpy \
        file \
        zlib1g-dev
```

On Ubuntu 20.04, install gcc-10 which has C++20 support:
```bash
sudo apt-get install -y g++-10
```

To build, make sure you are in the root of the `spring` repo, then run the following command:
```bash
mkdir -p build
cd build

## on Ubuntu 20, specify the gcc-10 compiler
cmake -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/lib/llvm-11 ..

## on Ubuntu 22, the default gcc version is 11, using the default compiler is fine
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/lib/llvm-11 ..

make -j "$(nproc)" package
```

Now you can optionally [test](#step-4---test) your build, or [install](#step-5---install) the `*.deb` binary packages, which will be in the root of your build directory.

### Step 4 - Test
Spring supports the following test suites:

Test Suite | Test Type | [Test Size](https://testing.googleblog.com/2010/12/test-sizes.html) | Notes
---|:---:|:---:|---
[Parallelizable tests](#parallelizable-tests) | Unit tests | Small
[WASM spec tests](#wasm-spec-tests) | Unit tests | Small | Unit tests for our WASM runtime, each short but _very_ CPU-intensive
[Serial tests](#serial-tests) | Component/Integration | Medium
[Long-running tests](#long-running-tests) | Integration | Medium-to-Large | Tests which take an extraordinarily long amount of time to run

When building from source, we recommended running at least the [parallelizable tests](#parallelizable-tests).

#### Parallelizable Tests
This test suite consists of any test that does not require shared resources, such as file descriptors, specific folders, or ports, and can therefore be run concurrently in different threads without side effects (hence, easily parallelized). These are mostly unit tests and [small tests](https://testing.googleblog.com/2010/12/test-sizes.html) which complete in a short amount of time.

You can invoke them by running `ctest` from a terminal in your Spring build directory and specifying the following arguments:
```bash
ctest -j "$(nproc)" -LE _tests
```

#### WASM Spec Tests
The WASM spec tests verify that our WASM execution engine is compliant with the web assembly standard. These are very [small](https://testing.googleblog.com/2010/12/test-sizes.html), very fast unit tests. However, there are over a thousand of them so the suite can take a little time to run. These tests are extremely CPU-intensive.

You can invoke them by running `ctest` from a terminal in your Spring build directory and specifying the following arguments:
```bash
ctest -j "$(nproc)" -L wasm_spec_tests
```
We have observed severe performance issues when multiple virtual machines are running this test suite on the same physical host at the same time, for example in a CICD system. This can be resolved by disabling hyperthreading on the host.

#### Serial Tests
The serial test suite consists of [medium](https://testing.googleblog.com/2010/12/test-sizes.html) component or integration tests that use specific paths, ports, rely on process names, or similar, and cannot be run concurrently with other tests. Serial tests can be sensitive to other software running on the same host and they may `SIGKILL` other `nodeos` processes. These tests take a moderate amount of time to complete, but we recommend running them.

You can invoke them by running `ctest` from a terminal in your Spring build directory and specifying the following arguments:
```bash
ctest -L "nonparallelizable_tests"
```

#### Long-Running Tests
The long-running tests are [medium-to-large](https://testing.googleblog.com/2010/12/test-sizes.html) integration tests that rely on shared resources and take a very long time to run.

You can invoke them by running `ctest` from a terminal in your Spring build directory and specifying the following arguments:
```bash
ctest -L "long_running_tests"
```

### Step 5 - Install
Once you have [built](#step-3---build-the-source-code) Spring and [tested](#step-4---test) your build, you can install Spring on your system. Don't forget to omit `sudo` if you are running in a docker container.

We recommend installing the binary package you just built. Navigate to your Spring build directory in a terminal and run this command:
```bash
sudo apt-get update
sudo apt-get install -y ./spring_*.deb
```

It is also possible to install using `make` instead:
```bash
sudo make install
```

## Bash Autocomplete
`cleos` and `spring-util` offer a substantial amount of functionality. Consider using bash's autocompletion support which makes it easier to discover all their various options.

For our provided `.deb` packages simply install Ubuntu's `bash-completion` package: `apt-get install bash-completion` (you may need to log out/in after installing).

If building from source install the `build/programs/cleos/bash-completion/completions/cleos` and `build/programs/spring-util/bash-completion/completions/spring-util` files to your bash-completion directory. Refer to [bash-completion's documentation](https://github.com/scop/bash-completion#faq) on the possible install locations.
