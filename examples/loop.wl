let items = ['Apple', 'Orange', 'Strawberry']

let list =
    <ul>
    \for item in items:
        <li>\{escape item}</li>
    </ul>

<html>
    <head>
        <title>Fruit List</title>
    </head>
    <body>
        My list:
        \{list}
    </body>
</html>
