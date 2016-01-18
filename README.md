# NatNetC
[NatNet](http://www.optitrack.com/products/natnet-sdk/) client library for accessing MoCap data on Linux/Debian.

## Features
Beside a clean C library for accessing/parsing NatNet protocol frames, this project is planned to provide the following functionalities:

1. on the fly conversion of NatNet frames to [YAML](http://pyyaml.org/wiki/LibYAML)
2. [LibFUSE](libfuse.2.dylib not loaded) interface on Linux
3. [OSXFuse](https://osxfuse.github.io) interface on Debian

# Status
Fully experimental and significantly incomplete.

# ToDo
## Project-level
- Choose a license
- Add a Build/Install guide on this file, including dependencies
- Write code documentation (I know this line is going to stay forever)

## NatNetC Library
- Add functionality for saving/logging raw NatNet frames
- On `NatNet_unpack_*()`
  - in `NatNet_unpack_yaml()` complete YAML emission also including skeletons
  - add optional logging in `NatNet_unpack_yaml()`
  - Veryfy and test conformity to different byte-endianness
  - add function for direct unpacking of single fields (markers, rigid bodies, etc)

## Fuse filesystem
- Implement a Fuse filesystem that exports NatNet frames as YAML files and as raw data binary files.
- Use write operations in the FUSE filesystem for changing parameters at run time.
- Test performance, i.e. provide a frame-rate value on different platforms. Reference platforms/OSs are Intel/OS X 10.11, Intel/Debian 8, and ARM/Raspibian.


# License
This software is provided according to the GPL General Public License v.3, according to the original License text available [here](http://www.gnu.org/licenses/gpl.html).

#Authors
Authors of this software are Paolo[dot]Bosetti and Mirko[dot]Brentari (both [at]unitn[dot]it).