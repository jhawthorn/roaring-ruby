# frozen_string_literal: true

require "mkmf"

#submodule = "#{__dir__}/roaring/"

#$objs = ["cext.o", "roaring.o"]
#$CPPFLAGS += " -I#{submodule} "

create_makefile("roaring/roaring")
