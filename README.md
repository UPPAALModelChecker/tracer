# tracer
Utility `tracer` reads Uppaal symbolic traces (`.xtr` "dot" files) and prints them in human readable form. 

Limitation: cannot handle traces which contain probabilistically branching edges. The issue is that the intermediate format cannot represent branch-points and branching edge identifiers do not match the syntactical edge identifiers in the original model.

## Example Usage
The following assumes that UPPAAL is installed and the `verifyta` utility is available

Given a model `cat-and-mouse.xml` compile the document into an intermediate format file `cat-and-mouse.if`:
```bash
UPPAAL_COMPILE_ONLY=1 verifyta cat-and-mouse.xml > cat-and-mouse.if 
```
Given a model `cat-and-mouse.xml` and a query in `cat-and-mouse-cheese.q` produce a diagnostic trace in `cat-and-mouse-1.xtr` (or alternatively use UPPAAL GUI simulator to save the symbolic trace):
```bash
verifyta -t0 -f cat-and-mouse cat-and-mouse.xml cat-and-mouse.q
```
Use `tracer` utility to read `cat-and-mouse.if` and `cat-and-mouse-1.xtr` and produce human-readable trace:
```bash
tracer cat-and-mouse.if cat-and-mouse-1.xtr
```
Example output (see [cat-and-mouse-1.txt](cat-and-mouse-1.txt)):
```txt
State: Cat.L0 Mouse.L13 CatP.Idle MouseP.Idle Cat.s=0 Mouse.s=13 #t(0)-#time<=0 #t(0)-time<=0 #t(0)-CatP.x<=0 #t(0)-MouseP.x<=0 #time-#t(0)<=1 #time-time<=0 time-CatP.x<=0 CatP.x-MouseP.x<=0 MouseP.x-#time<=0 

Transition: MouseP.Idle -> MouseP.Move {x >= MP; 0; x = 0;} 

State: Cat.L0 Mouse.L13 CatP.Idle MouseP.Move Cat.s=0 Mouse.s=13 #t(0)-#time<=-1 #t(0)-time<=0 #t(0)-CatP.x<=0 #t(0)-MouseP.x<=0 #time-time<=0 time-CatP.x<=0 CatP.x-MouseP.x<=1 MouseP.x-#t(0)<=0 

Transition: MouseP.Move -> MouseP.Idle {1; ml!; 1;} Mouse.L13 -> Mouse.L12 {1; ml?; s = 12;} 

State: Cat.L0 Mouse.L12 CatP.Idle MouseP.Idle Cat.s=0 Mouse.s=12 #t(0)-#time<=-1 #t(0)-time<=0 #t(0)-CatP.x<=0 #t(0)-MouseP.x<=0 #time-#t(0)<=2 #time-time<=0 time-CatP.x<=0 CatP.x-MouseP.x<=1 MouseP.x-#time<=-1 

Transition: CatP.Idle -> CatP.Move {x >= CP; 0; x = 0;} 

State: Cat.L0 Mouse.L12 CatP.Move MouseP.Idle Cat.s=0 Mouse.s=12 #t(0)-#time<=-2 #t(0)-time<=0 #t(0)-CatP.x<=0 #t(0)-MouseP.x<=0 #time-time<=0 time-CatP.x<=2 CatP.x-MouseP.x<=-1 MouseP.x-#t(0)<=1 
```

## Compile from Source
### Linux: Install Tools
```bash
sudo apt install git g++ cmake ninja-build
```

### Windows: Install Tools

- [Visual Studio Community](https://visualstudio.microsoft.com/vs/community/). 
  Make sure to select `Desktop development with C++`, `C++ CMake tools for Windows`
- [git](https://git-scm.com/download/win).
  Make sure to select `Add git to the PATH`.
- [CMake](https://cmake.org/download/). Make sure to select `Add cmake to the PATH`.

Start PowerShell (press `Win+R`, enter `pwsh`, press ENTER) and continue with compilation.

### macOS: Install Tools
- [Xcode](https://developer.apple.com/xcode/) from AppStore. Start at least once to ensure that command line tools are installed and license is accepted.
- [git](https://git-scm.com/download/mac)
- [CMake](https://cmake.org/download/)

### Compile from Terminal Shell
Checkout the source:
```bash
git clone https://github.com/UPPAALModelChecker/tracer.git
```
Generate `cmake` build system into a new `build` folder from `tracer` source folder:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release tracer
```
Compile:
```bash
cmake --build build --config Release
```
Test:
```bash
cd build
ctest -C Release --output-on-failure
```

### Development
Enable sanitizers to catch undefined behavior and memory issues by adding their options during the build generation:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUBSAN=ON -DASAN=ON -G Ninja tracer
cmake --build build --config Debug
cd build
ctest -C Debug --output-on-failure
```
