
<!--
    You can declare functions too.

    Unlike the global scope, expressions
    are not printed by default, so you need
    to use the print statement to do so.
-->

fun say_hello(name) {
    print "Hello to "
    print name
}

say_hello("cozis")

<!--
    If a function is implemented with a single
    expression, you can omit the curly braces
    to return it
-->

fun say_hello_2(name)
    <a>Hello, \name!</a>

say_hello_2("cozis")
