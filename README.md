## DIReducer
The code repository of  DIReducer (ASE'25)


## Environmental Requirements
- Ubuntu 22.04
- cmake 3.10 or later
- libboost-dev-all 1.73.0 or later


## Build 
```
mkdir build && cd build
cmake ..
make
```


## Run
**Note**: Use `DIReducer --help` to get all the options.
A simple example to process a binary is:
```
DIReducer --run  --input <input_file> --output <output_file>
```
- <input_file> is the input binary file
- <output_file> is the output binary file

In `example/` directory, there are some sample input files and expected output dir, you can run the following command to test the tool:
```
DIReducer --run --input example/input/expr --output example/output/expr
```
After finishing, the terminal will output the results as json format. As shown in the example below:
```
{
  "BaseInfo": {
    "program": "expr"
  },
  "Core": {
    "after": {
      "DI Size(KB)": 22,
      "P(T)": 5331
    },
    "before": {
      "DI Size(KB)": 148,
      "P(T)": 24895
    }
  },
  ...
}

```
- `program` is the name of the input binary file
- `DI Size(KB)` is the size of Debug Information
- `P(T)` is the Attributes of Debug Information