
let title = "My Website"

let posts = [
    {
        name: "Post Title 1",
        date: "1 Jan 2025",
        comments: [
            { author: "User 1", date: "1 Jan 2025", content: "This is a comment" },
            { author: "User 2", date: "2 Jan 2025", content: "Second comment" },
        ]
    },
    {
        name: "Post Title 2",
        date: "4 July 2022",
        comments: [
            { author: "User 1", date: "1 Jan 2025", content: "This is a comment" },
            { author: "User 2", date: "2 Jan 2025", content: "Second comment" },
        ]
    },
]

fun render_comment(c)
    <div class="comment">
        <a>\c.author (\c.date)</a>
        <p>\c.content</p>
        <div class="comment-children">
        \for child in c.children:
            render_comment(child)
        </div>
    </div>

<html>
    <head>
        <title>\title</title>
    </head>
    <body>
    <a>This is regular text</a>
    \for post in posts:
        <div class="post">
            <a>\post.title (\post.date)</a>
            \for comment in post.comments:
                render_comment(comment) 
        </div>
    </body>
</html>
