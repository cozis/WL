
procedure my_page_template(title, content) {
    <html>
        <head>
            <title>\{escape title}</title>
        </head>
        <body>
            \{content}
        </body>
    </html>
}

my_page_template("Welcome to my webpage!", <article>Content of my page</article>)
