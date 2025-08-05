
class Amalgamator:
    def __init__(self):
        self.out = ""

    def append_text(self, text):
        self.out += text

    def append_file(self, file):

        self.out += "\n"
        self.out += "////////////////////////////////////////////////////////////////////////////////////////\n"
        self.out += "// " + file + "\n"
        self.out += "////////////////////////////////////////////////////////////////////////////////////////\n"
        self.out += "\n"
        # self.out += "#line 1 \"" + file + "\"\n"
        self.out += open(file).read()

        if len(self.out) > 0 and self.out[len(self.out)-1] != '\n':
            self.out += "\n"

    def save(self, file):
        open(file, 'w').write(self.out)

desc = """
// This file was generated automatically. Do not modify directly!
"""

header = Amalgamator()
header.append_text("#ifndef WL_AMALGAMATION\n")
header.append_text("#define WL_AMALGAMATION\n")
header.append_text(desc)
header.append_file("src/public.h")
header.append_text("#endif // WL_AMALGAMATION\n")
header.save("WL.h")

source = Amalgamator()
source.append_text("#include \"WL.h\"\n")
source.append_file("src/includes.h")
source.append_file("src/basic.h")
source.append_file("src/basic.c")
source.append_file("src/file.h")
source.append_file("src/file.c")
source.append_file("src/parse.h")
source.append_file("src/parse.c")
source.append_file("src/assemble.h")
source.append_file("src/assemble.c")
source.append_file("src/value.h")
source.append_file("src/value.c")
source.append_file("src/eval.h")
source.append_file("src/eval.c")
source.append_file("src/public.c")
source.save("WL.c")
