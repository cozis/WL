<!--
    You can embed elements based on conditions
    using plain if-else statements
-->

let a = 2

if a == 1: {
    <a>The condition is true</a>
} else {
    <a>It is false</a>
}

<!--
    If the branch occurs inside an HTML element,
    you must escape the if keyword with a backslash
-->

<p>
    \if a == 1: {
        <a>The condition is true</a>
    } else {
        <a>It is false</a>
    }
</p>

<!--
    Similarly, you can execute code multiple times
    based on a condition using a while statement.

    The following loop generates the output

        <a>I'm link number 1</a>
        <a>I'm link number 2</a>
        <a>I'm link number 3</a>

    Note how the i variable is printed after adding
    1 to it.
-->

let i = 0
while i < 3: {
    <a>I'm link number \i + 1</a>
    i = i + 1
}

<!--
    To insert the loop in an HTML element, you need
    to escape it
-->

i = 0
<ul>
    \while i < 3: {
        <li>I'm link number \i + 1</li>
        i = i + 1
    }
</ul>