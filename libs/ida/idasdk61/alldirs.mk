$(ALLDIRS):
        @cd $@
        @$(MAKE)
        @cd ..