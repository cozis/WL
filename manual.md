# Expressions

A WL file is a sequence of statements. One type of statement is expressions:

```
1 + 2
```

All expressions statements are evaluated and written to output.

## Supported Types

WL supports these type of values:

1. None: A value which is only equal to itself
2. Booleans
3. Integers: Equivalent to `int64_t` in C
4. Floats: Equivalent to `double` in C
5. Strings: Sequence of bytes
6. Arrays: Etherogeneous sequences of values
7. Maps: Associations between arbitrary key-value paris

This is how the literals are used:

```
none
true
false
"I'm a string"
'I'm a string too!'
[1, 2, 3]
+{ "I'm the first key": 1, "I'm the second key": 2 }
```

## Unary Operators

The supported unary operators are

1. `+`: Allowed on any type and returns the operand unchanged
2. `-`: Negates an integer or float value
3. `len`: Returns the number of items stored into an array or the number of key-value pairs in a map

## Binary Operators

The supported binary operators are

1. `+`: Sums two numeric values. If a float value is involved, the result is a float too.
1. `-`: Subtracts two numeric values. If a float value is involved, the result is a float too.
1. `*`: Multiplies two numeric values. If a float value is involved, the result is a float too.
1. `/`: Divides two numeric values. If a float value is involved, the result is a float too.
1. `%`: Returns the division's remainder. The operands must be integers.
1. `==`: Returns `true` if the operands are the same, else returns `false`.
1. `!=`: Returns `true` if the operands are different, else returns `false`
1. `<`: Returns `true` if the first operand is lower than the second one, else returns `false`. The operands must be numeric.
1. `>`: Returns `true` if the first operand is greater than the second one, else returns `false`. The operands must be numeric.

Note that there are no implicit conversions, so for instance the integer `1` is different from the floating point `1.0`.

## Escaping Characters In String Literals

String literals can only contain printable ASCII characters (codepoints 32 to 127). Any other byte value must be escaped.

You can use `\n`, `\t`, `\r` to represent the line feed, horizontal tab, and carriage return characters.

Since single `'` or double `"` quotes are used as string delimiters, you must escape any quote that's part of the value using a backslash: `\'`, `\"`.

If a string contains a backslash, the backslash itself must be escaped `\\`.

Any byte value can be encoded using the `\x` notation

```
"This byte \xFF is not valid ASCII"
```

It allows to encode any byte value with its uppercase or lowercase hexadecimal representation. There must always be two hex digits, even if the high bits are zero.

## Variables & Scopes

You can bind expression results to variables

```
let a = 1+2
```

This will bind the result of the expression to the name "a". Variable names can contain digits, letters, and underscores, but the first character can't be a digit. When an expression is bound to a variable, it is not written to output.

You can later reuse the bound value by its variable name

```
a + 3
```

This will output `6`.

You can't declare two variables with the same time. The following is invalid:

```
let a = 1
let a = 2
```

You can reuse the same variable name by declaring a new scope:

```
let a = 1

{
    let a = 2
    a
}
```

Grouping statements into scopes this way allows one to reuse variable names. Whenever a variable is referenced, the one in the nearest scope is used. So the previous example will output `2`.

# If-else statements

You can optionally run some code based on an expression result using if-else statements:

```
if 1 > 2: {
    "First branch taken"
} else {
    "Second branch taken"
}
```

As usual, you can omit the else branch

```
if 1 > 2: {
    "Branch taken"
}
```

If the branch only contains one statement, you can omit the curly braces

```
if 1 > 2:
    "First branch taken"
else
    "Second branch taken"
```

A consequence of this is that you can chain if-else statements

```
let a = 4
if a == 1: {
    "a is 1"
} else if a == 2: {
    "a is 2"
} else if a == 3: {
    "a is 3"
} else {
    "a is something else"
}
```

# While statements

You can loop while a certain condition is true using a while statement

```
let i = 0
while i < 3: {
    "i="
    i
    "\n"
    i = i+1
}
```

Which will print:

```
i=0
i=1
i=2
```

# For statements

You can iterate over the items of an array using the for statement

```
for item in ["A", "B", "C"]: {
    item
}
```

This will print

```
ABC
```

By adding a second iteration variable, you will be able to read the current index

