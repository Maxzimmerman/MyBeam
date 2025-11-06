# MyBeam

## MyBeam is a mini version of the beam implementioin here: https://github.com/erlang/otp

## Features

- Load generic beam instructions into specific beam instructions
- Execute those specific beam structions

## Modules

### The Loader: Loads the generic beam instructions into specific beam instruction 

Responsibilities:
- Parse BEAM file header ("FOR1" "BEAM")
- Iterate through chunks (Atom, Code, ExpT, etc.)
- Register the module in a global module table (e.g. loaded_modules)

## The Interpreter: Executes BEAM instructions for one process.

Responsibilities:
- Fetch, decode, execute BEAM opcodes
- Manipulate registers, heap, stack
- Perform BEAM operations like move, call, send, receive, etc.
- Count reductions and yield to the scheduler


## Process: Implements the lightweight BEAM process abstraction.

Responsibilities:
- Create/destroy processes (spawn, exit)
- Allocate process-local heap and stack
- Manage mailbox for message passing
- Track reductions, instruction pointer, registers

## Scheduler: Implements cooperative multitasking among processes.

Responsibilities:
- Maintain a run queue of ready processes
- Select next process to run (round-robin or priority)
- Invoke the interpreter
- Handle yield/preemption based on reduction count

## Main runtime: Entry point that ties everything together.

Responsibilities:
- Initialize the runtime system (atom table, memory)
- Load .beam files via loader
- Spawn the initial process (Module:init/0 or equivalent)
- Start the scheduler loop

## Folder structure guideline

```
MYBEAM/
│
├── beam/                               # The Beam runntime
│   ├── loader.c                        # The Loader 
│   ├── interp.c                        # The Beam interpreter
│   ├── process.c                       # The Process
│   └── ...                             # Other Beam modules
│
├── input_files/                        # Elixir or Erlang files 
│   └── first_module.ex/                # Just an input elixir file containing a simple module
│
├── my_beam/                            # An elixir mix projects to debug compiled beam fies
│
├── output_files/                       # Compiled beam files
│   ├── Elixir.FirstModule.beam/        # Auth feature