# CLAFORD - clang-format-daemon

The program starts watching the specified directory and applies `clang-format -i -style=file ...` if a file gets modified.

![image](https://user-images.githubusercontent.com/4126943/229810574-c6241367-87c8-4ad2-b92e-f198e0b97940.png)

## Build

You need to have `conan 1.5*` and `cmake` installed.

Run these scripts:
```
./0_make_deps.sh
./1_config.sh # Optional args passed to cmake, e.g. ./1_config.sh -G Xcode
./2_make.sh
```

## Usage

```
./i/bin/claford <dir-to-watch>
```

Then click `Add All` to add all the files in the directory. Otherwise they will be added automaticallly when they change.
