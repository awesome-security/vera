
# this makefile creates the output directories for object/library files

objdir $(OBJDIR):
	@[ -d $(OBJDIR) ] || mkdir -p $(OBJDIR)
