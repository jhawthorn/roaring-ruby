# frozen_string_literal: true

require "mkmf"

$CFLAGS << " -fvisibility=hidden "

create_makefile("roaring/roaring")
