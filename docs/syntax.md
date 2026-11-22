# VekLang Syntax Guide

VekLang is a statically-typed, bytecode-compiled programming language running on a custom stack-based Virtual Machine. Declarations name types explicitly, programs begin at `func main() int`, and values are returned using `ret`.

## Variables, Constants, and Types

Use `let` for mutable variables and `const` for immutable values. Supported primitive types include `int`, `int32`, `int64`, `float`, `float32`, `float64`, `str`, `char`, and `bool` (plus `void` for functions that do not return a value).

```cpp
let int count = 3;
let int64 big_num = 1000000;
let float64 ratio = 0.75;
let bool active = true;
let char grade = 'A';
const str language = "VekLang";
```

Convert between types explicitly using C-style `(type)` casts:

```cpp
let int score = 42;
let float64 precise = (float64)score;
let str message = "Score: " + (str)score + "\n";
```

## Arrays

Declare dynamic arrays using `{ ... }` initializers or fixed-size arrays with `[size]`. Arrays support zero-based indexing (`[]`), `.push()`, and `.pop()`.

```cpp
let int scores[] = {90, 45, 85};
scores.push(100);
let int last = scores.pop();
let int first = scores[0];

let int buffer[3];
buffer[0] = 10;
```

## Control Flow and Loops

Use `if`, `else if`, and `else` with braces around each block. Standard comparison (`==`, `!=`, `<`, `>`, `<=`, `>=`) and logical (`&&`, `||`, `!`) operators are supported.

```cpp
if (count > 0 && active) {
    print("Positive and active\n");
} else if (count == 0) {
    print("Zero\n");
} else {
    print("Negative\n");
}
```

Use `while` loops for iteration, along with `break` to exit early and `continue` to skip to the next iteration:

```cpp
let int i = 0;
while (i < 10) {
    i = i + 1;
    if (i == 3) {
        continue;
    }
    if (i == 7) {
        break;
    }
    print((str)i + "\n");
}
```

## Functions and Pass-by-Reference

Functions place parameter types **before** parameter names and specify the return type **after** the parameter list. Use `ref` to pass a variable by reference so changes inside the function modify the caller's variable.

```cpp
func double_value(int value) int {
    ret value * 2;
}

func add_bonus(ref int total, int bonus) void {
    total = total + bonus;
}

func main() int {
    let int points = double_value(21);
    add_bonus(points, 8);
    print((str)points + "\n");
    ret 0;
}
```

## Console Input and Output

Use `print(...)` to write text to the console and `input(var)` to read user input directly into an existing variable.

```cpp
let str name = "";
let int num = 1;
print("Enter your name: ");
input(name);
print("Hello, " + str_trim(name) + "!\n");
print("I love number " + (str)(num) + "!\n"); // always cast to str/char!
```

## Modules

Split programs across multiple `.vek` files or import standard library modules with `load`:

```cpp
load random;
load "game/helpers";
```

## Standard Library and Natives

### Random (`load random;`)
* `random()` — Returns a `float64` in `[0.0, 1.0)`
* `rand(min, max)` — Returns a `float64` in `[min, max)`
* `rand_int(min, max)` — Returns an `int64` in `[min, max]` (inclusive)
* `rand_range(max)` — Returns an `int64` in `[0, max - 1]`
* `chance(p)` — Returns `true` with probability `p` (`0.0` to `1.0`)
* `sign()` — Returns `-1` or `1`
* `seed(s)` / `reset_seed()` — Sets a deterministic RNG seed or resets to system entropy

### Strings (Built-in Natives)
* `str_len(s)` — Returns string length as `int64`
* `str_trim(s)` — Removes leading and trailing whitespace
* `str_upper(s)` / `str_lower(s)` — Converts case
* `str_contains(s, sub)` / `str_starts_with(s, pre)` / `str_ends_with(s, suf)` — Substring checks (`bool`)
* `str_at(s, idx)` — Returns the `char` at index `idx`
* `str_substr(s, start, len)` — Extracts a substring
* `str_replace(s, from, to)` — Replaces all occurrences of `from` with `to`
* `str_is_int(s)` / `str_is_float(s)` — Safely checks whether a string can be parsed as a number
* `str_to_int(s)` / `str_to_float(s)` — Parses a string into `int64` or `float64`
