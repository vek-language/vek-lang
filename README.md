![VekLang Banner](https://repository-images.githubusercontent.com/1445189670/8c10a523-9cf8-4c74-96d8-c500ca53ff34)
---

# VekLang
![License](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)
![Language](https://img.shields.io/badge/Language-C%2B%2B20-purple.svg)
![Type](https://img.shields.io/badge/Type-Bytecode%20VM-success.svg)

**Vek** is a fast, statically-typed, bytecode-compiled programming language with its own custom stack-based Virtual Machine (similar to Java and the JVM). 
It is designed to be simple, bridging the syntax of C++ and Java with some of the flexibility seen in JavaScript.

## Prerequisites
* A C++ compiler supporting C++20 (GCC, Clang, or MSVC)
* CMake 3.16 or newer

## Cross-Platform Build (Windows, Linux, macOS):
```bash
# 1. Configure the project
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 2. Build the compiler (vek) and VM (vekvm)
cmake --build build --config Release
```

[![Download VekLang](https://img.shields.io/badge/Download-v0.0.2-red?style=for-the-badge&logo=windows)](https://github.com/vek-language/vek-lang/releases/latest)  
---
![Demo](https://github.com/user-attachments/assets/4cc4a84d-66de-484e-90d9-a2a4c8d13456)
---
You can see the docs [here](https://github.com/vek-language/vek-lang/tree/mavin/docs).

### Language Features
- [x] Variables & Math
- [x] If/Else Control Flow
- [x] Functions & Scopes
- [x] Dynamic Arrays (`push`, `pop`, indexing)
- [x] `while` and `for` loops
- [x] `&&` and `||`
- [x] Structs
- [x] String manipulation functions 
- [ ] Classes

**Code that shows capabilities (run with `./main {filename}.vek`)**
```cpp
load math; // loads std lib "math" (use quotes for user module)

func calculate_bonus(int score, int bonus_points) int {
    if (score > 80) {
        ret score + bonus_points;
    }
    ret score;
}

func main() int {
    // Variables and Constants
    const int MAX_SCORE = 100;
    let str title = "VekLang";
    let bool is_awesome = true;

    print(title);
    print(is_awesome);

    // Arrays (Dynamic)
    let int scores[] = {90, 85, 70}; // or scores[3] - those are same
    scores.push(95); // Add an item
    let int last = scores.pop(); // Remove an item

    // Control Flow
    for (let int i = 0; i < 3; i += 1) { // Also `while` loops exist too.
        print(scores[i]);
    }

    // Functions
    let int final_score = calculate_bonus(scores[0], 10);
    
    if (final_score == MAX_SCORE) {
        print("Perfect score!");
    } else {
        print("Good job!");
    }

    ret 0;
}
```

## TODO
- [ ] Organize src .cpp files instead of using .hpp only.
- [ ] Add classes.
- [x] Implement while/for loops.
- [x] CMake file.
- [x] Compiler and VM supposed to be different things. (_vekvm {filename}.vekbc_ to run, _vek run/build_ - developer tool) (turn .vekbc is a bytecode file, with magic, 3 bytes in header.)

## Acknowledgements
This project was built using AI tools as an occasional educational aid and a minor coding assistant (primarily for boilerplate and formatting). 
The overall architecture, core compiler/VM design, implementation, and final debugging were entirely driven and executed by the author.
