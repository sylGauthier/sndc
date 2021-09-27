# sndc

`sndc` is a modular, non real time synthesizer that essentially behaves like a
compiler for sound effects.

It turns source files (`.sndc` files), which are essentially directed acyclic
graphs of data transferred from generators through various effect modules into a
float buffer which can then be converted to sound files or played with external
tools.

This is supposed to be a programmer/nerd friendly way of quickly hacking
together complex sound effects and potentially even music, with source files
that can be easily added to a VCS like git, and generation that's build-system
friendly when integrated into a complex project such as a video game.

## Build

Dependencies:

 - libfftw3

```
$ git clone github.com/sylGauthier/sndc.git
$ cd sndc
$ make
```

You can install `sndc` and its wrappers with `make install`.

## Write a sound effect source

A source file is a collection of nodes. Each node is an instance of a particular
module. A node is declared like so:

```
nodeName: moduleName {
    /* list of inputs */
}
```

Each module has a set of inputs, whose type can be either `FLOAT`, `BUFFER` or
`STRING`. Of course this is an ongoing project and further data types will be
added in the future. They are set like so:

```
    inputName: inputValue;
```

Some inputs are required and some are optional depending on the module.
`inputValue` can be either a literal value, or the output of a node **above**
the current node. Example:

```
newNode: myModule { /* node named "newNode", instance of "myModule" */
    input: n.out;   /* "input" is set to an output buffer of a previous node */
    freq: 440;      /* "freq" is set to a literal float */
    interp: "sine"; /* "interp" is set to a literal string */
}
```

For a better overview, look at the examples in the `examples` directory.

You can list all built-in modules with:

```
$ ./sndc -l
```

You can list inputs of a particular module, along with their type and whether
they are required, with:

```
$ ./sndc -h moduleName
```

## Play/export sound

`sndc` follows the UNIX philosophy and as such, delegates the task of playing
and converting the output binary data to external tools.

Linux users will find that tools such as `aplay` for playing and `sox` for
converting raw output are a good fit.

`sndc` will go through the input file, load all modules in memory, perform basic
sanity checks of input types, then print the raw content of the output of the
last module in the stack, either in the specified output file or in `stdout`.

A typical invocation of `sndc` piped into `aplay` for immediate replay of the
sound might look like this:

```
$ ./sndc file.sndc | aplay -c 1 -t raw -r 44100 -f float_le
```

Note that the `float_le` is specific for little endian machines, big endian
machines should use `float_be` instead.

Similarly, to convert the output into whatever audio format the user desires,
one should use `sox` with an invocation such as:

```
$ ./sndc file.sndc | sox -t raw -r 44100 -c 1 -L -e floating-point -b 32 - out.wav
```

Some convenient wrappers are located in the `wrappers` directory, they get
installed along with `sndc` when typing `make install`.

To play a `sndc` file using the wrapper over `aplay`:

```
$ sndc_play <sndcfile>
```

To export using the wrapper over `sox`:

```
$ sndc_export <sndcfile> <audiofile>
```

## Notes

This is an ongoing project with heaps of improvements yet to do, but the basic
idea is more or less already there.
