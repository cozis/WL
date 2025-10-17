
let comments = [
    {
        'content': 'Cool post!',
        'children': [
            {
                'content': 'True!',
                'children': []
            }
        ]
    },
    {
        'content': 'I don\'t agree with the second section..',
        'children': [
            {
                'content': 'You\'re wrong!',
                'children': [
                    {
                        'content': 'No, YOU are wrong!',
                        'children' : []
                    }
                ]
            }
        ]
    },
]

procedure render_comment_subtree(comment) {
    <div class="comment">
        <p>\{escape comment.content}</p>
        <div class="comment-children">
        \for child in comment.children:
            render_comment_subtree(child)
        </div>
    </div>
}

<html>
    <head>
        <meta charset="UTF-8" />
        <style>
        .comment {
            border-left: 2px solid #ccc;
            padding-left: 10px;
        }
        </style>
    </head>
    <body>
    <article>

        <h1>A Very Cool Post</h1>

        <h2>Section 1</h2>

        <p>
            This language is cool
        </p>

        <h2>Section 2</h2>

        <p>
            You should try it out
        </p>

    </article>

    <div id="comments">
    \for comment in comments:
        render_comment_subtree(comment)
    </div>

    </body>
</html>
