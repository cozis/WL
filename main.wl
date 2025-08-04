
fun comment(data)
    <div>
        <a>\data.author</a>
        <p>\data.content</p>
        \for sub_comment in data.sub_comments:
            comment(sub_comment)
    </div>

comment()