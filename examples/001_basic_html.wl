<!--
    You can have regular HTML elements in WL.
-->

<a head="some_page.html">I'm a link</a>

<!--
    You can declare variables and use them in the HTML by
    escaping the name
-->

let name = "cozis"

<p>Hello from \name</p>

<!--
    HTML attributes are evaluated as WL expressions.

    The following evaluates to

        <p A=Some value B=7></p>

    which, to be fair, isn't right. There shoud be quotes
    around "Some value"
-->

let valueA = "Some value"

<p A=valueA B=1+2*3></p>

<!--
    HTML attributes are just expressions, and therefore
    can be assigned to variables
-->

let link = <a href="page.html">Click me</a>

<!--
    And then can be printed by simply stating the name
    of the variable or by embedding it in a lager element
-->

link

<p>You should click this link: \link</p>
