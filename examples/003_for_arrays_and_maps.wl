
<!--
    WL supports integers, floats, strings, array and map
    values. Arrays are what you expect. They allow one to
    store sequences of elements.

    The following snippet prints

        123

    which is the string obtained by concatenating all
    the elements
-->

let my_var = [1, 2, 3]

my_var

<!--
    Maps are similar to Python dicts and Javascript
    objects. They store associations between values

    The following prints

        Second
-->

let my_map = { 1: "First", 2: "Second", 3: "Third" }

my_map[2]

<!--
    You can have any type as a map key, and if the
    key is a string that is also a valid variable
    name, you can drop the double quotes
-->

let person = { "name": "Cozis" }
let person_no_quotes = { name: "Cozis" }

<!--
    You can iterate over the keys of a map using the
    for loop. The following prints the string

        ABC

-->

for key in { A: 1, B: 2, C: 3 }: {
    key
}

for key, i in { A: 1, B: 2, C: 3 }: {
    <!--
        You can keep track of the current index by adding
        a second interation variable
     -->
}

<!--
    When using a for loop over a map, the first
    iteration variable holds its keys. When the
    iterated value is an array, the first variable
    returns its values
-->

for val in [5, 3, 7]:
    val

for val, i in [5, 3, 7]:
    i

<!--
    As usual, you can have for statements in HTML
    by escaping them
-->

let links = ["http://github.com", "http://reddit.com"]

<ul>
    \for link, i in links:
        <li><a href=link>I'm link number \i</a></li>
</ul>
