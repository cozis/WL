# WL
WL is the proof of concept for a powerful and flexible templating language for the web. All functionality is implemented, but is still missing some polish.

Here's an example template in WL with a recursive comment view element:
```html
let posts = [
    {
        author: "UserA",
        title: "I'm the first post",
        content: "Sup everyone!",
        date: "1 Jan 2025",
        comments: [
            {
                author: "UserB",
                content: "Hello! This is a comment!",
                comments: [
                    {
                        author: "UserA",
                        content: "Hello to you!",
                        comments: []
                    }
                ]
            }
        ]
    }
]

fun comment(c)
    <div>
        <a>\c.author</a>
        <p>\c.content</p>
        \for subc in c.comments:
            comment(subc)
    </div>

<html>
    <head>
        <title>List of posts</title>
    </head>
    <body>
        \for post in posts:
            <div class="post">
                <h2>
                    \post.title
                </h2>
                <span>
                    Posted on \post.date by \post.author
                </span>
                <p>
                    \post.content
                </p>
                <div class="comments">
                    \for c in post.comments:
                        comment(c)
                </div>
            </div>
    </body>
</html>
```