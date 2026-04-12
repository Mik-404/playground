ifdef VERBOSE
ld_colored = @echo -e  "   \033[1m\033[33m[LD]   \033[37m$@\033[0m $(1)" && $(LD) $1
as_colored = @echo -e  "   \033[1m\033[35m[AS]   \033[37m${MODULE}/$@\033[0m $(1)" && $(AS) $1
cxx_colored = @echo -e "   \033[1m\033[34m[CXX]  \033[37m$@\033[0m $(1)" && $(CXX) $1

else
ld_colored = @echo -e  "   \033[1m\033[33m[LD]   \033[37m$@\033[0m" && $(LD) $1
as_colored = @echo -e  "   \033[1m\033[35m[AS]   \033[37m${MODULE}/$@\033[0m" && $(AS) $1
cxx_colored = @echo -e "   \033[1m\033[34m[CXX]  \033[37m${MODULE}/$@\033[0m" && $(CXX) $1

endif
