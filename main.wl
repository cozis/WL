
func(1, 3, 5);

title = "My Website";

posts = [
    { name: "Post Title 1", data: "1 Jan 2025" },
    { name: "Post Title 2", data: "1 May 2024" },
];

<html>
    <head>
        <title>$(title);</title>
    </head>
    <body>
        <ul>
        $ for post in posts:
            <li>$(post.name); - $(post.date);</li>
        </ul>
        $ comment(1, 3, 4);
    </body>
</html>
