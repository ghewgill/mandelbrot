env = Environment(CCFLAGS = "-I/usr/local/include", LIBPATH = "/usr/local/lib")
env.Append(CXXFLAGS = "-O3")
env.Program("tile.cgi", "tile.cpp", LIBS = ["gd", "fcgi"])
