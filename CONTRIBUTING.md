# Contributing to VekLang
First off, thank you for considering contributing to VekLang! It's people like you that make open-source great.

## Project Structure
* `vek/` — Compiler frontend (`lexer.hpp`, `parser.hpp`, `compiler.hpp`, `AST.hpp`)
* `vekvm/` — Stack-based Virtual Machine
* `vekp/` — Package manager CLI
* `shared/` — Shared bytecode, definitions, serializer, and diagnostics (`bytecode.hpp`, `token.hpp`)

## How Can I Contribute?

### Reporting Bugs
* Check the [Issues](https://github.com/vek-language/vek-lang/issues/) tab to see if the bug has already been reported.
* If not, open a new issue. Describe what you expected to happen, what actually happened, and include a small `.vek` code snippet that reproduces the bug.

### Suggesting Features
* Open an issue and describe the feature you want to see. Explain *why* it would be useful for the language.

### Submitting Pull Requests (PRs)
1. Fork the repository.
2. Create a new branch (`git checkout -b feat/your-feature`).
3. Make your changes.
4. Build and test your changes locally using CMake (C++20 required):
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
5. Run a test `.vek` script with the compiled binaries (`./build/vek run test.vek`).
6. Commit your changes and push to your branch.
7. Open a Pull Request!

## Looking for a Place to Start?
* Check out the issues labeled [`good first issue`](https://github.com) or [`help wanted`](https://github.com).

## Questions?
* If you have any questions about the compiler architecture or how something works, feel free to open an issue or discussion. Don't worry if you are new to C++ or compiler design—we're happy to help!
