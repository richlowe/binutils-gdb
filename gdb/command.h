/* Header file for command creation.

   Copyright (C) 1986-2019 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#if !defined (COMMAND_H)
#define COMMAND_H 1

#include "common/gdb_vecs.h"
#include "common/scoped_restore.h"

struct completion_tracker;

/* This file defines the public interface for any code wanting to
   create commands.  */

/* Command classes are top-level categories into which commands are
   broken down for "help" purposes.

   Notes on classes: class_alias is for alias commands which are not
   abbreviations of the original command.  class-pseudo is for
   commands which are not really commands nor help topics ("stop").  */

enum command_class
{
  /* Special args to help_list */
  class_deprecated = -3, all_classes = -2, all_commands = -1,
  /* Classes of commands */
  no_class = -1, class_run = 0, class_vars, class_stack, class_files,
  class_support, class_info, class_breakpoint, class_trace,
  class_alias, class_bookmark, class_obscure, class_maintenance,
  class_pseudo, class_tui, class_user, class_xdb,
  no_set_class	/* Used for "show" commands that have no corresponding
		   "set" command.  */
};

/* FIXME: cagney/2002-03-17: Once cmd_type() has been removed, ``enum
   cmd_types'' can be moved from "command.h" to "cli-decode.h".  */
/* Not a set/show command.  Note that some commands which begin with
   "set" or "show" might be in this category, if their syntax does
   not fall into one of the following categories.  */
typedef enum cmd_types
  {
    not_set_cmd,
    set_cmd,
    show_cmd
  }
cmd_types;

/* Types of "set" or "show" command.  */
typedef enum var_types
  {
    /* "on" or "off".  *VAR is an integer which is nonzero for on,
       zero for off.  */
    var_boolean,

    /* "on" / "true" / "enable" or "off" / "false" / "disable" or
       "auto.  *VAR is an ``enum auto_boolean''.  NOTE: In general a
       custom show command will need to be implemented - one that for
       "auto" prints both the "auto" and the current auto-selected
       value.  */
    var_auto_boolean,

    /* Unsigned Integer.  *VAR is an unsigned int.  The user can type
       0 to mean "unlimited", which is stored in *VAR as UINT_MAX.  */
    var_uinteger,

    /* Like var_uinteger but signed.  *VAR is an int.  The user can
       type 0 to mean "unlimited", which is stored in *VAR as
       INT_MAX.  The only remaining use of it is the Python API.
       Don't use it elsewhere.  */
    var_integer,

    /* String which the user enters with escapes (e.g. the user types
       \n and it is a real newline in the stored string).
       *VAR is a malloc'd string, or NULL if the string is empty.  */
    var_string,
    /* String which stores what the user types verbatim.
       *VAR is a malloc'd string, or NULL if the string is empty.  */
    var_string_noescape,
    /* String which stores a filename.  (*VAR) is a malloc'd string,
       or "" if the string was empty.  */
    var_optional_filename,
    /* String which stores a filename.  (*VAR) is a malloc'd
       string.  */
    var_filename,
    /* ZeroableInteger.  *VAR is an int.  Like var_integer except
       that zero really means zero.  */
    var_zinteger,
    /* ZeroableUnsignedInteger.  *VAR is an unsigned int.  Zero really
       means zero.  */
    var_zuinteger,
    /* ZeroableUnsignedInteger with unlimited value.  *VAR is an int,
       but its range is [0, INT_MAX].  -1 stands for unlimited and
       other negative numbers are not allowed.  */
    var_zuinteger_unlimited,
    /* Enumerated type.  Can only have one of the specified values.
       *VAR is a char pointer to the name of the element that we
       find.  */
    var_enum
  }
var_types;

/* This structure records one command'd definition.  */
struct cmd_list_element;

typedef void cmd_const_cfunc_ftype (const char *args, int from_tty);

/* This structure specifies notifications to be suppressed by a cli
   command interpreter.  */

struct cli_suppress_notification
{
  /* Inferior, thread, frame selected notification suppressed?  */
  int user_selected_context;
};

extern struct cli_suppress_notification cli_suppress_notification;

/* Forward-declarations of the entry-points of cli/cli-decode.c.  */

/* API to the manipulation of command lists.  */

extern bool valid_user_defined_cmd_name_p (const char *name);

/* Const-correct variant of the above.  */

extern struct cmd_list_element *add_cmd (const char *, enum command_class,
					 cmd_const_cfunc_ftype *fun,
					 const char *,
					 struct cmd_list_element **);

/* Like add_cmd, but no command function is specified.  */

extern struct cmd_list_element *add_cmd (const char *, enum command_class,
					 const char *,
					 struct cmd_list_element **);

extern struct cmd_list_element *add_cmd_suppress_notification
			(const char *name, enum command_class theclass,
			 cmd_const_cfunc_ftype *fun, const char *doc,
			 struct cmd_list_element **list,
			 int *suppress_notification);

extern struct cmd_list_element *add_alias_cmd (const char *, const char *,
					       enum command_class, int,
					       struct cmd_list_element **);

extern struct cmd_list_element *add_alias_cmd (const char *,
					       cmd_list_element *,
					       enum command_class, int,
					       struct cmd_list_element **);


extern struct cmd_list_element *add_prefix_cmd (const char *, enum command_class,
						cmd_const_cfunc_ftype *fun,
						const char *,
						struct cmd_list_element **,
						const char *, int,
						struct cmd_list_element **);

extern struct cmd_list_element *add_prefix_cmd_suppress_notification
			(const char *name, enum command_class theclass,
			 cmd_const_cfunc_ftype *fun,
			 const char *doc, struct cmd_list_element **prefixlist,
			 const char *prefixname, int allow_unknown,
			 struct cmd_list_element **list,
			 int *suppress_notification);

extern struct cmd_list_element *add_abbrev_prefix_cmd (const char *,
						       enum command_class,
						       cmd_const_cfunc_ftype *fun,
						       const char *,
						       struct cmd_list_element
						       **, const char *, int,
						       struct cmd_list_element
						       **);

typedef void cmd_const_sfunc_ftype (const char *args, int from_tty,
				    struct cmd_list_element *c);
extern void set_cmd_sfunc (struct cmd_list_element *cmd,
			   cmd_const_sfunc_ftype *sfunc);

/* A completion routine.  Add possible completions to tracker.

   TEXT is the text beyond what was matched for the command itself
   (leading whitespace is skipped).  It stops where we are supposed to
   stop completing (rl_point) and is '\0' terminated.  WORD points in
   the same buffer as TEXT, and completions should be returned
   relative to this position.  For example, suppose TEXT is "foo" and
   we want to complete to "foobar".  If WORD is "oo", return "oobar";
   if WORD is "baz/foo", return "baz/foobar".  */
typedef void completer_ftype (struct cmd_list_element *,
			      completion_tracker &tracker,
			      const char *text, const char *word);

/* Same, but for set_cmd_completer_handle_brkchars.  */
typedef void completer_handle_brkchars_ftype (struct cmd_list_element *,
					      completion_tracker &tracker,
					      const char *text, const char *word);

extern void set_cmd_completer (struct cmd_list_element *, completer_ftype *);

/* Set the completer_handle_brkchars callback.  */

extern void set_cmd_completer_handle_brkchars (struct cmd_list_element *,
					       completer_handle_brkchars_ftype *);

/* HACK: cagney/2002-02-23: Code, mostly in tracepoints.c, grubs
   around in cmd objects to test the value of the commands sfunc().  */
extern int cmd_cfunc_eq (struct cmd_list_element *cmd,
			 cmd_const_cfunc_ftype *cfun);

/* Each command object has a local context attached to it.  */
extern void set_cmd_context (struct cmd_list_element *cmd,
			     void *context);
extern void *get_cmd_context (struct cmd_list_element *cmd);


/* Execute CMD's pre/post hook.  Throw an error if the command fails.
   If already executing this pre/post hook, or there is no pre/post
   hook, the call is silently ignored.  */
extern void execute_cmd_pre_hook (struct cmd_list_element *cmd);
extern void execute_cmd_post_hook (struct cmd_list_element *cmd);

/* Return the type of the command.  */
extern enum cmd_types cmd_type (struct cmd_list_element *cmd);

/* Flag for an ambiguous cmd_list result.  */
#define CMD_LIST_AMBIGUOUS ((struct cmd_list_element *) -1)

extern struct cmd_list_element *lookup_cmd (const char **,
					    struct cmd_list_element *,
					    const char *,
					    int, int);

extern struct cmd_list_element *lookup_cmd_1 (const char **,
					      struct cmd_list_element *,
					      struct cmd_list_element **,
					      int);

extern struct cmd_list_element *deprecate_cmd (struct cmd_list_element *,
					       const char * );

extern void deprecated_cmd_warning (const char *);

extern int lookup_cmd_composition (const char *text,
				   struct cmd_list_element **alias,
				   struct cmd_list_element **prefix_cmd,
				   struct cmd_list_element **cmd);

extern struct cmd_list_element *add_com (const char *, enum command_class,
					 cmd_const_cfunc_ftype *fun,
					 const char *);

extern struct cmd_list_element *add_com_alias (const char *, const char *,
					       enum command_class, int);

extern struct cmd_list_element *add_com_suppress_notification
		       (const char *name, enum command_class theclass,
			cmd_const_cfunc_ftype *fun, const char *doc,
			int *supress_notification);

extern struct cmd_list_element *add_info (const char *,
					  cmd_const_cfunc_ftype *fun,
					  const char *);

extern struct cmd_list_element *add_info_alias (const char *, const char *,
						int);

extern void complete_on_cmdlist (struct cmd_list_element *,
				 completion_tracker &tracker,
				 const char *, const char *, int);

extern void complete_on_enum (completion_tracker &tracker,
			      const char *const *enumlist,
			      const char *, const char *);

/* Functions that implement commands about CLI commands.  */

extern void help_list (struct cmd_list_element *, const char *,
		       enum command_class, struct ui_file *);

/* Method for show a set/show variable's VALUE on FILE.  If this
   method isn't supplied deprecated_show_value_hack() is called (which
   is not good).  */
typedef void (show_value_ftype) (struct ui_file *file,
				 int from_tty,
				 struct cmd_list_element *cmd,
				 const char *value);
/* NOTE: i18n: This function is not i18n friendly.  Callers should
   instead print the value out directly.  */
extern show_value_ftype deprecated_show_value_hack;

extern void add_setshow_enum_cmd (const char *name,
				  enum command_class theclass,
				  const char *const *enumlist,
				  const char **var,
				  const char *set_doc,
				  const char *show_doc,
				  const char *help_doc,
				  cmd_const_sfunc_ftype *set_func,
				  show_value_ftype *show_func,
				  struct cmd_list_element **set_list,
				  struct cmd_list_element **show_list,
				  void *context = nullptr);

extern void add_setshow_auto_boolean_cmd (const char *name,
					  enum command_class theclass,
					  enum auto_boolean *var,
					  const char *set_doc,
					  const char *show_doc,
					  const char *help_doc,
					  cmd_const_sfunc_ftype *set_func,
					  show_value_ftype *show_func,
					  struct cmd_list_element **set_list,
					  struct cmd_list_element **show_list);

extern void add_setshow_boolean_cmd (const char *name,
				     enum command_class theclass,
				     int *var,
				     const char *set_doc, const char *show_doc,
				     const char *help_doc,
				     cmd_const_sfunc_ftype *set_func,
				     show_value_ftype *show_func,
				     struct cmd_list_element **set_list,
				     struct cmd_list_element **show_list);

extern void add_setshow_filename_cmd (const char *name,
				      enum command_class theclass,
				      char **var,
				      const char *set_doc,
				      const char *show_doc,
				      const char *help_doc,
				      cmd_const_sfunc_ftype *set_func,
				      show_value_ftype *show_func,
				      struct cmd_list_element **set_list,
				      struct cmd_list_element **show_list);

extern void add_setshow_string_cmd (const char *name,
				    enum command_class theclass,
				    char **var,
				    const char *set_doc,
				    const char *show_doc,
				    const char *help_doc,
				    cmd_const_sfunc_ftype *set_func,
				    show_value_ftype *show_func,
				    struct cmd_list_element **set_list,
				    struct cmd_list_element **show_list);

extern struct cmd_list_element *add_setshow_string_noescape_cmd
		      (const char *name,
		       enum command_class theclass,
		       char **var,
		       const char *set_doc,
		       const char *show_doc,
		       const char *help_doc,
		       cmd_const_sfunc_ftype *set_func,
		       show_value_ftype *show_func,
		       struct cmd_list_element **set_list,
		       struct cmd_list_element **show_list);

extern void add_setshow_optional_filename_cmd (const char *name,
					       enum command_class theclass,
					       char **var,
					       const char *set_doc,
					       const char *show_doc,
					       const char *help_doc,
					       cmd_const_sfunc_ftype *set_func,
					       show_value_ftype *show_func,
					       struct cmd_list_element **set_list,
					       struct cmd_list_element **show_list);

extern void add_setshow_integer_cmd (const char *name,
				     enum command_class theclass,
				     int *var,
				     const char *set_doc,
				     const char *show_doc,
				     const char *help_doc,
				     cmd_const_sfunc_ftype *set_func,
				     show_value_ftype *show_func,
				     struct cmd_list_element **set_list,
				     struct cmd_list_element **show_list);

extern void add_setshow_uinteger_cmd (const char *name,
				      enum command_class theclass,
				      unsigned int *var,
				      const char *set_doc,
				      const char *show_doc,
				      const char *help_doc,
				      cmd_const_sfunc_ftype *set_func,
				      show_value_ftype *show_func,
				      struct cmd_list_element **set_list,
				      struct cmd_list_element **show_list);

extern void add_setshow_zinteger_cmd (const char *name,
				      enum command_class theclass,
				      int *var,
				      const char *set_doc,
				      const char *show_doc,
				      const char *help_doc,
				      cmd_const_sfunc_ftype *set_func,
				      show_value_ftype *show_func,
				      struct cmd_list_element **set_list,
				      struct cmd_list_element **show_list);

extern void add_setshow_zuinteger_cmd (const char *name,
				       enum command_class theclass,
				       unsigned int *var,
				       const char *set_doc,
				       const char *show_doc,
				       const char *help_doc,
				       cmd_const_sfunc_ftype *set_func,
				       show_value_ftype *show_func,
				       struct cmd_list_element **set_list,
				       struct cmd_list_element **show_list);

extern void
  add_setshow_zuinteger_unlimited_cmd (const char *name,
				       enum command_class theclass,
				       int *var,
				       const char *set_doc,
				       const char *show_doc,
				       const char *help_doc,
				       cmd_const_sfunc_ftype *set_func,
				       show_value_ftype *show_func,
				       struct cmd_list_element **set_list,
				       struct cmd_list_element **show_list);

/* Do a "show" command for each thing on a command list.  */

extern void cmd_show_list (struct cmd_list_element *, int, const char *);

/* Used everywhere whenever at least one parameter is required and
   none is specified.  */

extern void error_no_arg (const char *) ATTRIBUTE_NORETURN;


/* Command line saving and repetition.
   Each input line executed is saved to possibly be repeated either
   when the user types an empty line, or be repeated by a command
   that wants to repeat the previously executed command.  The below
   functions control command repetition.  */

/* Commands call dont_repeat if they do not want to be repeated by null
   lines or by repeat_previous ().  */

extern void dont_repeat ();

/* Commands call repeat_previous if they want to repeat the previous command.
   Such commands that repeat the previous command must indicate
   to not repeat themselves, to avoid recursive repeat.
   repeat_previous will mark the current command as not repeating,
   and will ensure get_saved_command_line returns the previous command,
   so that the currently executing command can repeat it.  */

extern void repeat_previous ();

/* Prevent dont_repeat from working, and return a cleanup that
   restores the previous state.  */

extern scoped_restore_tmpl<int> prevent_dont_repeat (void);

/* Set the arguments that will be passed if the current command is
   repeated.  Note that the passed-in string must be a constant.  */

extern void set_repeat_arguments (const char *args);

/* Returns the saved command line to repeat.
   When a command is being executed, this is the currently executing
   command line, unless the currently executing command has called
   repeat_previous (): in this case, get_saved_command_line returns
   the previously saved command line.  */

extern char *get_saved_command_line ();

/* Takes a copy of CMD, for possible repetition.  */

extern void save_command_line (const char *cmd);

/* Used to mark commands that don't do anything.  If we just leave the
   function field NULL, the command is interpreted as a help topic, or
   as a class of commands.  */

extern void not_just_help_class_command (const char *, int);

/* Check function pointer.  */
extern int cmd_func_p (struct cmd_list_element *cmd);

/* Call the command function.  */
extern void cmd_func (struct cmd_list_element *cmd,
		      const char *args, int from_tty);

#endif /* !defined (COMMAND_H) */
