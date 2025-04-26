# NPB3.3-MZ (Multi-Zone) OpenMP Versions

This repository contains  C versions of NPB3.3-MZ benchmarks. The C versions have been translated from original Fortran versions in a semi-automatic ways.

These benchmarks should be compiled with natural C compilers, do not use CP Tools and composable runtimes because not all necessary OpenMP features are supported at the moment.

## The Original Benchmark Details

NPB Development Team
   npb@nas.nasa.gov


This is the nested OpenMP implementation of the multi-zone
NAS Parallel Benchmarks.  For detail descriptions of the benchmarks,
please see:
   http://www.nas.nasa.gov/Research/Reports/Techreports/2003/
   nas-03-010-abstract.html


This file briefly summarizes features and changes made in
different releases of the OpenMP versions.  For release history 
of the multi-zone benchmarks, please refer to Changes.log.

For benchmark compilation and runtime setup, please refer to
README.install included in the directory.


NPB 3.3-MZ-OMP is the first release of the nested OpenMP 
implementation of the multi-zone benchmarks.  It replaces
the SMP+OpenMP implementation included in previous releases.
This release is merged with the vector codes.  The vector version 
can be selected with the "VERSION=VEC" option during compilation.
For other details, please refer to README.install.

## Build and Run

Modify/specify the compiler and compilation flags in `config/make.def`. In `config` directory you can find more examples of `config/make.def` files.

```bash
  # Create the directory to build executables.
  mkdir bin

  # Build benchmarks, where <benchmark> is one of `bt-mz`, `sp-mz`, `lu-mz`, <class> is one of S, W, A through F.
  make <benchmark> CLASS=<class>
```

If some errors occur on the first compilation attempt. Try to run the command above again.

Execute a benchmark with the command:

```bash
  OMP_NUM_THREADS=<outer> OMP_NUM_THREADS2=<innter> ./bin/<bencmark>.<class>.x
```

Use the following variables to control the number of threads at each level of parallelism:

* OMP_NUM_THREADS - the number of outer-level threads. If this variable is not specified,
    one thread is assumed.
* OMP_NUM_THREADS2 - the number of threads per outer-level thread. If this variable is not
    specified, one inner thread is assumed for each outer thread.

The total number of threads is number_of_othreads * number_inner_threads.

Note, that the execution of the lu-mz benchmark may require the preliminary configuration of the stack size, at least use

```bash
ulimit -s unlimited
```

You can also use `runtest` script to iterate over different number of threads at inner and outer levels

```bash
./runtest <benchmark>.<class>.x
```

or use `runall` script to execute A, B, C classes of each test (bt-mz, lu-mz, sp-mz), note that you should build all versions before the execution.
