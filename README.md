# WL
WL is a powerful and flexible, yet experimental scripting language for templating with first-class support for HTML.

## Features
1. *Zero dependencies* - It only uses pure C and the standard library
2. *Single-file implementation* - Everything is inside `wl.c` and `wl.h`
3. *HTML-first design* - Native HTML syntax with embedded scripting 
4. *Complete scripting language* - Variables, functions, loops, conditional branches, arrays, maps. We've got it all!
5. Built-in XSS protection - `escape()` function to sanitize dynamic HTML
6. *No I/O or dynamic allocations* - Any I/O or memory management is left to the user
7. *Include system* - Modular template composition over multiple files

## Language

WL is designed to be extremely powerful and flexible, but realistically you will use a small number of features most of the time. For instance, I expect most templates to look something like this:

```
let title = "Title of my webpage"
let items = ["A", "B", "C"]

let navigator = <nav>
        <a href="/home">Home</a>
        <a href="/about">About</a>
    </nav>

<html>
    <head>
        <title>\title</title>
    </head>
    <body>
    \navigator
    \for item in items:
        item
    </body>
</html>
```

But really, WL is a full fledged scripting languages, so let's start from the beginning...

### Expressions

WL supports integer, floats, booleans, strings, arrays, and maps values

```
100
4.5
true
false
"Hello, world!"
[1, 2, 3]
+{'name': 'Francesco', 'greeting': 'sup'}
```

Evaluating expressions in the global scope will automatically write them to output. One thing you may not expect is that when arrays are printed out, their contents are just concatenated and printed out. This is useful for doing lazy string manipulation. Map on the other hand, are not considered as printable objects and will only output `<map>`.

Your usual arithmetic expressions in infix notation are supported:

```
1 + (2 - 3) * 4 / 5
```

And behave as you would expect. Expressions on integers return integers with the exception of division which returns a float. Any operation involving a float returns a float too.

These are the available comparison operators:

```
1 < 2
1 > 2
1 == 2
1 != 2
```

The `<=` and `>=` operators are missing as I never needed them up to now!

You can use the `len` operator to get the number of items in arrays and maps

```
len [1, 2, 3]
len {'name': 'Francesco', 'greeting': 'sup'}
```

This will output 3 for the array and 2 for the map.

To select an item from an array or map, use the `[]` notation

```
([1, 2, 3])[1]
({'name': 'Francesco', 'greeting': 'sup'})['name']
```

### Variables

You can use the `let` keyword to declare variables

```
let A

A = 1
A = "Hello"
```

Variables are not bound to a type, so you can assign different type of values to them. You can output the variable's contents by just naming it

```
let name = "Alice"

"My name is "
name
```

You can only refer to a variable within its scope, and you can create new scopes using curly braces:

```
let A

{
    let B
    "B is available here"
}

"B is not available here"
"A can be accessed from anywhere"
```

You can also shadow variables by redeclaring them in nested scopes.

### Conditional branches and loops

As every other procedural language you can use if-else branches

```
if 1 < 2: {
    "First branch"
} else {
    "Second branch"
}
```

And you can use `for` loops to iterate over arrays and maps

```
for elem in ["A", "B", "C"]: {
    elem
}

let my_map = {"name": "Alice", "surname": "Smith"}
for key in my_map: {
    key
    my_map[key]
}
```

The iteration variable for arrays is the item itself, while the iteration variable for maps contains the current key. If you want to keep track of the interation index, you can declare a second iteration variable:

```
for elem, index in ["A", "B", "C"]: {
    index
}
```

You can also use regular while loops:

```
let i = 0
while i < 10: {
    "Current index is"
    i
    "\n"
    i = i+1
}
```

### Procedures

You can make code reusable by declaring procedures. For instance, the following procedure simply outputs its three arguments

```
procedure my_proc(a, b, c) {
    a
    b
    c
}

my_proc("A", true, -1)
```

You can also store the output of a procedure into a variable

```
let result = my_proc(1, 2, 3)
```

Note that procedures may very well be recursive

```
procedure factorial(n) {
    if n == 0: {
        1
    } else {
        n * factorial(n-1)
    }
}

factorial(10)
```

### HTML literals

HTML elements are also valid expressions

```
<p>This is some text</p>
```

You can have any text between HTML tags, with the exception of the backspace character `\` which is used to insert dynamic content into the element. You can use it to evaluate variables, if-else branches, loops, or anything really:

```
let name = "Francesco"
let fruit = ["Orange", "Apple"]

<p>
    My name is \name and these are the fruits I like

    <ul>
    \for item in fruit:
        <li>\fruit</li>
    </ul>
</p>
```

Since HTML literals are just expressions, you can also assign them to variables:

```
let links = {
    "home": "home.html",
    "about": "about.html",
    "contacts": "contacts.html"
}
let navigator =
    <nav>
        \for name in links:
            <a href=\links[name]>\name</a>
    </nav>
```

### File inclusion

You can include files using the `include` keyword.

Say you have a file `file_A.wl` containing some symbol definitions or outputs:

```
let myvar = 100

"Some output here"
```

You can import the symbols and generate the output from another file by including the first one

```
include "file_A.wl"

"myvar is accessible here: "
myvar
```