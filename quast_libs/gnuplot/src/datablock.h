/*
 * $Id: datablock.h,v 1.3 2014/04/05 06:17:08 markisch Exp $
 */
void datablock_command __PROTO((void));
char **get_datablock __PROTO((char *name));
char *parse_datablock_name __PROTO((void));
void gpfree_datablock __PROTO((struct value *datablock_value));
void append_to_datablock(struct value *datablock_value, const char * line);
