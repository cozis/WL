# WL Language Manual

## WL Programs

A WL program is composed by zero or more statements, which may include

1. Expressions
2. If-else conditionals
3. While loops
4. For loops
5. Variable declarations
6. Procedure declarations
7. Code Blocks
8. External file inclusions

Note that statements have no separators. To avoid ambiguities, it is useful to split statements over multiple lines.

The output of a program is defined as the cumulative results of all expressions evaluated in the global scope.

## Expressions

Programs operate on a set of types that one would expect from a scripting language:
* integers
* floats
* booleans
* none (equivalent to null)
* strings
* arrays
* maps (equivalent to dicts in Python and associative arrays in PHP)

### Integers and Floats

Integers are encoded as 64 bits signed, and therefore can encode the same value as an `int64_t` in C.

Similarly, floats are also encoded over 64 bits and can encode the same precision of a `double` in C.

The usual aritmethic operators are available

```
3.5 + 4
3.5 - 4
3.5 * 4
3.5 / 4
```

Aritmethic operations between integers, with the exception of division, also return integers. Any operation involving a float, or a division, will also return a float.

The modulo operator is also available for integers, which returns the division remainder

```
101 % 10
```

You can use the unary plus and minus to express the sign of a numeric literal

```
+100
-100
```

The plus can generally be omitted as it does nothing.

### Booleans

Boolean values work as you would expect. These are the true and false literals:

```
true
false
```

You can use comparison operators to produce true and false values

```
1 < 2
1 > 2
1 == 2
1 != 2
```

Note that the `>=` and `<=` operators are not implemented yet as I never needed them up to now. The same goes for `and` and `or`.

The ordering operators `<` and `>` may only be used on numeric values, while `==` and `!=` are allowed on any type.

### None

The none value is usually referred to as "null" or "nil" in other languages. It's only quality is being equal to itself and may be used to encode the absence of a value.

You can encode the none literal using the keyword
```
none
```

### Strings

Strings are sequence of characters delimited by single or double quotes. They can only contain printable ASCII characters (bytes from 32 to 127).

```
"Hello, world!"
'Hello, world!'
```

Any character outside the printable ASCII range must be encoded with its hexadecimal representation prefixed by `\x`. For instance, let's say we wanted to write the FF byte

```
"This byte is not printable: \xFF"
```

Some special characters that are not within the printable range may be escaped using their own specific escape sequence

```
"This is a newline \n"
"This is an horizontal tab \t"
"This is a carriage return \r"
```

Although the `'` and `"` characters are printable, they may interfere with the string delimiters, therefore strings the contain `'` characters should be delimited with `"` and viceversa. When avoiding the ambiguity is not possible, the `'` and `"` may themselves be escaped

```
" This is a double quote \" "
' This is a single quote \' '
```

Since the `\` character is used to escape other character, when used as its own character it also needs to be escaped

```
" This is a backslash \\ "
```

Note that the requirement that strings only contain printable characters implies that strings can't span multiple lines of source code. In other words, this doesn't work:

```
"This
is
not
valid"
```

### Arrays

Arrays represent an ordered sequence of values which may have different types from each other. They are defined using square brackets and commas as separators.

```
[1, 2.3, none]
```

Trailing commas are supported

```
[1, 2, 3,]
```

To insert new values into an array you use the shovel operator

```
[1, 2, 3] << 4
```

When evaluated into the global scope, arrays are output as the concatenation of their elements, which means they are useful to perform lazy concatenation of strings.

To select an element by its index (indices start at zero) you use the []-style selection. For instance the following returns `2`:

```
[1, 2, 3][1]
```

### Maps

Maps are the equivalent of dicts in Python and use a similar syntax:

```
+{ 'name': 'Alice', 'color': 'red' }
```

They are defined within curly braces and contain a list of zero or more key-value pairs separated by colons and commas. The `+` at the start is not part of the map, it's only used in the global scope to disambiguate with curly braces used for creating scopes and maps. By using an unary `+` we make explicit that we are writing an expression. Map objects are generally not considered as printable, and therefore defining them as we are would just output the string `<map>`.

Just like arrays, maps allow trailing commas:

```
+{ 'name': 'Alice', 'color': 'red', }
```

Map keys can have any type (except for other maps and arrays), but when a given key is a string and a valid identifier name, the quotes can be dropped:

```
+{ name: 'Alice', color: 'red' }
```

A valid identifier name is any sequence of digits, letters and underscores that does not start with a digit.

To select an item from a map, you can use the [] notation

```
+{ name: 'Alice', color: 'red' }["name"]
```

or the dot notation if the key you are selecting is a valid identifier

```
+{ name: 'Alice', color: 'red' }.name
```

You can assign new values to the given keys by using the `=` operator

```
+{ name: 'Alice', color: 'red' }["name"] = 'Bob'
+{ name: 'Alice', color: 'red' }.name = 'Bob'
```

## Variables

You can create variables to reuse results of expressions multiple times. Variables are declared using the `let` keyword and values can be assigned to them using the `=` operator

```
let name

name = "Alice"
```

You can also declare a variable and assign a value to it in the same statement

```
let name = "Alice"
```

If you didn't assign a value to a variable, its value will be `none`.

You can assign values to a variable multiple times, and the values may have different types

```
let name

name = "Alice"
name = "Bob"
```

Whenever you would use a numeric, string or any other literal, you name the variable holding the value instead.

## Code Blocks

You can group multiple statements into one code block statement using curly braces

```
{
    let a = 2
    3 * a
}
```

This has a couple effects. The first one, it allows you to treat multiple statements as one. This will come in handy with if-else statements. The second one, is that code blocks create variable "scopes". When a variable is declared within a scope, it can only be accessed by that scope or children scopes.

```
{
    let a = 2
    "a is accessible here"

    {
        "It's also accessible from here"
    }
}

"But not here"
```

This allows one to use the same variable name by declaring different scopes for it

```
{
    let name = "Alice"
}

{
    let name = "Bob"
}
```

If these variables were declared within the same scope, the program would be invalid.

Shadowing is also supported. You can declare a variable over another from a parent scope

```
let name = "Alice"

{
    let name = "Bob"
    name
}

name
```

This program will print "BobAlice". The inner scope declares a new verstion of `name` which *shadows* the first version. When the inner scope completes, the first version of `name` is not shadowed anymore and can be used again.

## If-Else Conditionals

You can execute an optional blocks of code based on whether an expression is true or not by using an if statement:

```
let a = 3

if a > 5: {
    "a is indeed greater than 5"
}
```

If you also want to execute an optional block of code when the condition is not true, you can add the block after the `else` keyword

```
let a = 3

if a > 5: {
    "a is indeed greater than 5"
} else {
    "a is not greater than 5..."
}
```

The curly braces are used to define code blocks, which means you can omit the braces when the block is only made by one statement

```
let a = 3

if a > 5:
    "a is indeed greater than 5"
else
    "a is not greater than 5..."
```

An implication of this is that you can chain multiple if statements like this:

```
let a = 3

if a > 5: {
    "a is indeed greater than 5"
} else if a > 4: {
    "a is greater than 4"
} else if a > 3: {
    "a is greater than 3"
} else {
    "a is not greater than 5..."
}
```

Note that variables declared within if-else blocks will not be accessible in the parent scope

## While loops

While loops are similar to if statements, except they evaluate their body while the condition is true

```
let i = 0
while i < 3: {
    "Print me"
    i = i + 1
}
```

This will print "Print me" three times.

## For loops

For loops are similar to while loop except they are intended to read elements contained by an array or map.

The following code will iterate over the elements of the array and print them

```
let A = [1, 2, 3]

for element in my_array: {
    element
}
```

If you want to keep track of the current iteration index, you can declare a second iteration variable

```
let A = [1, 2, 3]

for element, index in my_array: {
    element
    index
}
```

In a similar way you can iterate over maps

```
let A = { name: 'Alice', color: 'red' }

for key in A: {
    key
}
```

The first iteration variable returns the current key. To get the value associated to that key in the map, you need to read it explicitly using the key.

If, while reading over a map, you want to access the current iteration index, you can add a second iteration variable

```
let A = { name: 'Alice', color: 'red' }

for key, index in A: {
    key
    index
}
```

## Procedures

Unlike other languages, WL does not implement functions. It does implement procedures, which share commonalities but are overall different.

Here is an example of a procedure:

```
procedure greet(name) {
    "Hello from "
    name
    "!"
}

greet("Francesco")
```

The output of the procedure is the cumulative output of all expressions it evaluates.

Note that you may call procedures before their declaration

```
do_something()

procedure do_something() {
    "works!"
}
```

Procedures may call themselves recursively

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

## HTML literals

HTML literals are a type of expression, but they are complex enough that they warrant their own section.

Generally speaking, HTML elements are valid expressions

```
<p>Hello, world!</p>
```

which evaluate to a string containing that HTML. The content of an element can be an arbitrary string or nested HTML elements.

You can render variable values or any expression inside the HTML using the `\{}` notation:

```
let name = "Alice"

<p>My name is \{name} and my favourite number is \{99 + 1}!</p>
```

You can also evaluate optional HTML elements using the `\if` construct

```
let a = 5

<div>
    \if a < 10:
        <p>a is lower than 10</p>
    else
        <p>a isn't lower than 10</p>
</div>
```

And iterate over elements of arrays and maps using `\for`

```
let items = [1, 2, 3]

<ul>
    \for item in items:
        <li>\{item}</li>
</ul>
```

In reality, the `\` character works by interrupting the HTML content string to evaluate any one WL statement. The output of that statement is then added to the HTML element's output.

Since HTML elements are just expressions, you may assign them to variables

```
let items = [1, 2, 3]

let list =
    <ul>
        \for item in items:
            <li>\{item}</li>
    </ul>

list
```

Evaluating dynamic content within the opening tag is also allowed

```
let element_id = "mydiv"

<div id="\{element_id}"></div>
```

You may omit the closing tag of any element by using this syntax:

```
<div />
```

Note that WL treats all elements the same way and does not apply different behavior based on the specific tag it's parsing. For instance most browsers will allor you to have a single open tag `<br>` with no closing one since `br` doesn't expect children elements. This would be incorrect in WL. You need to either write the element as `<br></br>` or as `<br />`.