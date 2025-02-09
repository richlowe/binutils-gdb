/* GDB CLI commands.

   Copyright (C) 2000-2019 Free Software Foundation, Inc.

   This file is part of GDB.

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

#include "defs.h"
#include "arch-utils.h"
#include "readline/readline.h"
#include "readline/tilde.h"
#include "completer.h"
#include "target.h"	/* For baud_rate, remote_debug and remote_timeout.  */
#include "common/gdb_wait.h"	/* For shell escape implementation.  */
#include "gdbcmd.h"
#include "gdb_regex.h"	/* Used by apropos_command.  */
#include "gdb_vfork.h"
#include "linespec.h"
#include "expression.h"
#include "frame.h"
#include "value.h"
#include "language.h"
#include "filenames.h"	/* For DOSish file names.  */
#include "objfiles.h"
#include "source.h"
#include "disasm.h"
#include "tracepoint.h"
#include "common/filestuff.h"
#include "location.h"
#include "block.h"

#include "ui-out.h"
#include "interps.h"

#include "top.h"
#include "cli/cli-decode.h"
#include "cli/cli-script.h"
#include "cli/cli-setshow.h"
#include "cli/cli-cmds.h"
#include "cli/cli-utils.h"

#include "extension.h"
#include "common/pathstuff.h"

#ifdef TUI
#include "tui/tui.h"	/* For tui_active et.al.  */
#endif

#include <fcntl.h>
#include <algorithm>
#include <string>

/* Prototypes for local utility functions */

static void print_sal_location (const symtab_and_line &sal);

static void ambiguous_line_spec (gdb::array_view<const symtab_and_line> sals,
				 const char *format, ...)
  ATTRIBUTE_PRINTF (2, 3);

static void filter_sals (std::vector<symtab_and_line> &);


/* Limit the call depth of user-defined commands */
unsigned int max_user_call_depth;

/* Define all cmd_list_elements.  */

/* Chain containing all defined commands.  */

struct cmd_list_element *cmdlist;

/* Chain containing all defined info subcommands.  */

struct cmd_list_element *infolist;

/* Chain containing all defined enable subcommands.  */

struct cmd_list_element *enablelist;

/* Chain containing all defined disable subcommands.  */

struct cmd_list_element *disablelist;

/* Chain containing all defined stop subcommands.  */

struct cmd_list_element *stoplist;

/* Chain containing all defined delete subcommands.  */

struct cmd_list_element *deletelist;

/* Chain containing all defined detach subcommands.  */

struct cmd_list_element *detachlist;

/* Chain containing all defined kill subcommands.  */

struct cmd_list_element *killlist;

/* Chain containing all defined set subcommands */

struct cmd_list_element *setlist;

/* Chain containing all defined unset subcommands */

struct cmd_list_element *unsetlist;

/* Chain containing all defined show subcommands.  */

struct cmd_list_element *showlist;

/* Chain containing all defined \"set history\".  */

struct cmd_list_element *sethistlist;

/* Chain containing all defined \"show history\".  */

struct cmd_list_element *showhistlist;

/* Chain containing all defined \"unset history\".  */

struct cmd_list_element *unsethistlist;

/* Chain containing all defined maintenance subcommands.  */

struct cmd_list_element *maintenancelist;

/* Chain containing all defined "maintenance info" subcommands.  */

struct cmd_list_element *maintenanceinfolist;

/* Chain containing all defined "maintenance print" subcommands.  */

struct cmd_list_element *maintenanceprintlist;

/* Chain containing all defined "maintenance check" subcommands.  */

struct cmd_list_element *maintenancechecklist;

struct cmd_list_element *setprintlist;

struct cmd_list_element *showprintlist;

struct cmd_list_element *setdebuglist;

struct cmd_list_element *showdebuglist;

struct cmd_list_element *setchecklist;

struct cmd_list_element *showchecklist;

/* Command tracing state.  */

int source_verbose = 0;
int trace_commands = 0;

/* 'script-extension' option support.  */

static const char script_ext_off[] = "off";
static const char script_ext_soft[] = "soft";
static const char script_ext_strict[] = "strict";

static const char *const script_ext_enums[] = {
  script_ext_off,
  script_ext_soft,
  script_ext_strict,
  NULL
};

static const char *script_ext_mode = script_ext_soft;

/* Utility used everywhere when at least one argument is needed and
   none is supplied.  */

void
error_no_arg (const char *why)
{
  error (_("Argument required (%s)."), why);
}

/* The "info" command is defined as a prefix, with allow_unknown = 0.
   Therefore, its own definition is called only for "info" with no
   args.  */

static void
info_command (const char *arg, int from_tty)
{
  printf_unfiltered (_("\"info\" must be followed by "
		       "the name of an info command.\n"));
  help_list (infolist, "info ", all_commands, gdb_stdout);
}

/* The "show" command with no arguments shows all the settings.  */

static void
show_command (const char *arg, int from_tty)
{
  cmd_show_list (showlist, from_tty, "");
}


/* Provide documentation on command or list given by COMMAND.  FROM_TTY
   is ignored.  */

static void
help_command (const char *command, int from_tty)
{
  help_cmd (command, gdb_stdout);
}


/* Note: The "complete" command is used by Emacs to implement completion.
   [Is that why this function writes output with *_unfiltered?]  */

static void
complete_command (const char *arg, int from_tty)
{
  dont_repeat ();

  if (max_completions == 0)
    {
      /* Only print this for non-mi frontends.  An MI frontend may not
	 be able to handle this.  */
      if (!current_uiout->is_mi_like_p ())
	{
	  printf_unfiltered (_("max-completions is zero,"
			       " completion is disabled.\n"));
	}
      return;
    }

  if (arg == NULL)
    arg = "";

  int quote_char = '\0';
  const char *word;

  completion_result result = complete (arg, &word, &quote_char);

  if (result.number_matches != 0)
    {
      std::string arg_prefix (arg, word - arg);

      if (result.number_matches == 1)
	printf_unfiltered ("%s%s\n", arg_prefix.c_str (), result.match_list[0]);
      else
	{
	  result.sort_match_list ();

	  for (size_t i = 0; i < result.number_matches; i++)
	    {
	      printf_unfiltered ("%s%s",
				 arg_prefix.c_str (),
				 result.match_list[i + 1]);
	      if (quote_char)
		printf_unfiltered ("%c", quote_char);
	      printf_unfiltered ("\n");
	    }
	}

      if (result.number_matches == max_completions)
	{
	  /* ARG_PREFIX and WORD are included in the output so that emacs
	     will include the message in the output.  */
	  printf_unfiltered (_("%s%s %s\n"),
			     arg_prefix.c_str (), word,
			     get_max_completions_reached_message ());
	}
    }
}

int
is_complete_command (struct cmd_list_element *c)
{
  return cmd_cfunc_eq (c, complete_command);
}

static void
show_version (const char *args, int from_tty)
{
  print_gdb_version (gdb_stdout, true);
  printf_filtered ("\n");
}

static void
show_configuration (const char *args, int from_tty)
{
  print_gdb_configuration (gdb_stdout);
}

/* Handle the quit command.  */

void
quit_command (const char *args, int from_tty)
{
  int exit_code = 0;

  /* An optional expression may be used to cause gdb to terminate with
     the value of that expression.  */
  if (args)
    {
      struct value *val = parse_and_eval (args);

      exit_code = (int) value_as_long (val);
    }

  if (!quit_confirm ())
    error (_("Not confirmed."));

  query_if_trace_running (from_tty);

  quit_force (args ? &exit_code : NULL, from_tty);
}

static void
pwd_command (const char *args, int from_tty)
{
  if (args)
    error (_("The \"pwd\" command does not take an argument: %s"), args);

  gdb::unique_xmalloc_ptr<char> cwd (getcwd (NULL, 0));

  if (cwd == NULL)
    error (_("Error finding name of working directory: %s"),
           safe_strerror (errno));

  if (strcmp (cwd.get (), current_directory) != 0)
    printf_unfiltered (_("Working directory %s\n (canonically %s).\n"),
		       current_directory, cwd.get ());
  else
    printf_unfiltered (_("Working directory %s.\n"), current_directory);
}

void
cd_command (const char *dir, int from_tty)
{
  int len;
  /* Found something other than leading repetitions of "/..".  */
  int found_real_path;
  char *p;

  /* If the new directory is absolute, repeat is a no-op; if relative,
     repeat might be useful but is more likely to be a mistake.  */
  dont_repeat ();

  gdb::unique_xmalloc_ptr<char> dir_holder
    (tilde_expand (dir != NULL ? dir : "~"));
  dir = dir_holder.get ();

  if (chdir (dir) < 0)
    perror_with_name (dir);

#ifdef HAVE_DOS_BASED_FILE_SYSTEM
  /* There's too much mess with DOSish names like "d:", "d:.",
     "d:./foo" etc.  Instead of having lots of special #ifdef'ed code,
     simply get the canonicalized name of the current directory.  */
  gdb::unique_xmalloc_ptr<char> cwd (getcwd (NULL, 0));
  dir = cwd.get ();
#endif

  len = strlen (dir);
  if (IS_DIR_SEPARATOR (dir[len - 1]))
    {
      /* Remove the trailing slash unless this is a root directory
         (including a drive letter on non-Unix systems).  */
      if (!(len == 1)		/* "/" */
#ifdef HAVE_DOS_BASED_FILE_SYSTEM
	  && !(len == 3 && dir[1] == ':') /* "d:/" */
#endif
	  )
	len--;
    }

  dir_holder.reset (savestring (dir, len));
  if (IS_ABSOLUTE_PATH (dir_holder.get ()))
    {
      xfree (current_directory);
      current_directory = dir_holder.release ();
    }
  else
    {
      if (IS_DIR_SEPARATOR (current_directory[strlen (current_directory) - 1]))
	current_directory = concat (current_directory, dir_holder.get (),
				    (char *) NULL);
      else
	current_directory = concat (current_directory, SLASH_STRING,
				    dir_holder.get (), (char *) NULL);
    }

  /* Now simplify any occurrences of `.' and `..' in the pathname.  */

  found_real_path = 0;
  for (p = current_directory; *p;)
    {
      if (IS_DIR_SEPARATOR (p[0]) && p[1] == '.'
	  && (p[2] == 0 || IS_DIR_SEPARATOR (p[2])))
	memmove (p, p + 2, strlen (p + 2) + 1);
      else if (IS_DIR_SEPARATOR (p[0]) && p[1] == '.' && p[2] == '.'
	       && (p[3] == 0 || IS_DIR_SEPARATOR (p[3])))
	{
	  if (found_real_path)
	    {
	      /* Search backwards for the directory just before the "/.."
	         and obliterate it and the "/..".  */
	      char *q = p;

	      while (q != current_directory && !IS_DIR_SEPARATOR (q[-1]))
		--q;

	      if (q == current_directory)
		/* current_directory is
		   a relative pathname ("can't happen"--leave it alone).  */
		++p;
	      else
		{
		  memmove (q - 1, p + 3, strlen (p + 3) + 1);
		  p = q - 1;
		}
	    }
	  else
	    /* We are dealing with leading repetitions of "/..", for
	       example "/../..", which is the Mach super-root.  */
	    p += 3;
	}
      else
	{
	  found_real_path = 1;
	  ++p;
	}
    }

  forget_cached_source_info ();

  if (from_tty)
    pwd_command ((char *) 0, 1);
}

/* Show the current value of the 'script-extension' option.  */

static void
show_script_ext_mode (struct ui_file *file, int from_tty,
		     struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file,
		    _("Script filename extension recognition is \"%s\".\n"),
		    value);
}

/* Try to open SCRIPT_FILE.
   If successful, the full path name is stored in *FULL_PATHP,
   and the stream is returned.
   If not successful, return NULL; errno is set for the last file
   we tried to open.

   If SEARCH_PATH is non-zero, and the file isn't found in cwd,
   search for it in the source search path.  */

gdb::optional<open_script>
find_and_open_script (const char *script_file, int search_path)
{
  int fd;
  openp_flags search_flags = OPF_TRY_CWD_FIRST | OPF_RETURN_REALPATH;
  gdb::optional<open_script> opened;

  gdb::unique_xmalloc_ptr<char> file (tilde_expand (script_file));

  if (search_path)
    search_flags |= OPF_SEARCH_IN_PATH;

  /* Search for and open 'file' on the search path used for source
     files.  Put the full location in *FULL_PATHP.  */
  gdb::unique_xmalloc_ptr<char> full_path;
  fd = openp (source_path, search_flags,
	      file.get (), O_RDONLY, &full_path);

  if (fd == -1)
    return opened;

  FILE *result = fdopen (fd, FOPEN_RT);
  if (result == NULL)
    {
      int save_errno = errno;

      close (fd);
      errno = save_errno;
    }
  else
    opened.emplace (gdb_file_up (result), std::move (full_path));

  return opened;
}

/* Load script FILE, which has already been opened as STREAM.
   FILE_TO_OPEN is the form of FILE to use if one needs to open the file.
   This is provided as FILE may have been found via the source search path.
   An important thing to note here is that FILE may be a symlink to a file
   with a different or non-existing suffix, and thus one cannot infer the
   extension language from FILE_TO_OPEN.  */

static void
source_script_from_stream (FILE *stream, const char *file,
			   const char *file_to_open)
{
  if (script_ext_mode != script_ext_off)
    {
      const struct extension_language_defn *extlang
	= get_ext_lang_of_file (file);

      if (extlang != NULL)
	{
	  if (ext_lang_present_p (extlang))
	    {
	      script_sourcer_func *sourcer
		= ext_lang_script_sourcer (extlang);

	      gdb_assert (sourcer != NULL);
	      sourcer (extlang, stream, file_to_open);
	      return;
	    }
	  else if (script_ext_mode == script_ext_soft)
	    {
	      /* Assume the file is a gdb script.
		 This is handled below.  */
	    }
	  else
	    throw_ext_lang_unsupported (extlang);
	}
    }

  script_from_file (stream, file);
}

/* Worker to perform the "source" command.
   Load script FILE.
   If SEARCH_PATH is non-zero, and the file isn't found in cwd,
   search for it in the source search path.  */

static void
source_script_with_search (const char *file, int from_tty, int search_path)
{

  if (file == NULL || *file == 0)
    error (_("source command requires file name of file to source."));

  gdb::optional<open_script> opened = find_and_open_script (file, search_path);
  if (!opened)
    {
      /* The script wasn't found, or was otherwise inaccessible.
         If the source command was invoked interactively, throw an
	 error.  Otherwise (e.g. if it was invoked by a script),
	 just emit a warning, rather than cause an error.  */
      if (from_tty)
	perror_with_name (file);
      else
	{
	  perror_warning_with_name (file);
	  return;
	}
    }

  /* The python support reopens the file, so we need to pass full_path here
     in case the file was found on the search path.  It's useful to do this
     anyway so that error messages show the actual file used.  But only do
     this if we (may have) used search_path, as printing the full path in
     errors for the non-search case can be more noise than signal.  */
  source_script_from_stream (opened->stream.get (), file,
			     search_path ? opened->full_path.get () : file);
}

/* Wrapper around source_script_with_search to export it to main.c
   for use in loading .gdbinit scripts.  */

void
source_script (const char *file, int from_tty)
{
  source_script_with_search (file, from_tty, 0);
}

static void
source_command (const char *args, int from_tty)
{
  const char *file = args;
  int search_path = 0;

  scoped_restore save_source_verbose = make_scoped_restore (&source_verbose);

  /* -v causes the source command to run in verbose mode.
     -s causes the file to be searched in the source search path,
     even if the file name contains a '/'.
     We still have to be able to handle filenames with spaces in a
     backward compatible way, so buildargv is not appropriate.  */

  if (args)
    {
      while (args[0] != '\0')
	{
	  /* Make sure leading white space does not break the
	     comparisons.  */
	  args = skip_spaces (args);

	  if (args[0] != '-')
	    break;

	  if (args[1] == 'v' && isspace (args[2]))
	    {
	      source_verbose = 1;

	      /* Skip passed -v.  */
	      args = &args[3];
	    }
	  else if (args[1] == 's' && isspace (args[2]))
	    {
	      search_path = 1;

	      /* Skip passed -s.  */
	      args = &args[3];
	    }
	  else
	    break;
	}

      file = skip_spaces (args);
    }

  source_script_with_search (file, from_tty, search_path);
}


static void
echo_command (const char *text, int from_tty)
{
  const char *p = text;
  int c;

  if (text)
    while ((c = *p++) != '\0')
      {
	if (c == '\\')
	  {
	    /* \ at end of argument is used after spaces
	       so they won't be lost.  */
	    if (*p == 0)
	      return;

	    c = parse_escape (get_current_arch (), &p);
	    if (c >= 0)
	      printf_filtered ("%c", c);
	  }
	else
	  printf_filtered ("%c", c);
      }

  reset_terminal_style (gdb_stdout);

  /* Force this output to appear now.  */
  wrap_here ("");
  gdb_flush (gdb_stdout);
}

/* Sets the last launched shell command convenience variables based on
   EXIT_STATUS.  */

static void
exit_status_set_internal_vars (int exit_status)
{
  struct internalvar *var_code = lookup_internalvar ("_shell_exitcode");
  struct internalvar *var_signal = lookup_internalvar ("_shell_exitsignal");

  clear_internalvar (var_code);
  clear_internalvar (var_signal);
  if (WIFEXITED (exit_status))
    set_internalvar_integer (var_code, WEXITSTATUS (exit_status));
  else if (WIFSIGNALED (exit_status))
    set_internalvar_integer (var_signal, WTERMSIG (exit_status));
  else
    warning (_("unexpected shell command exit status %d\n"), exit_status);
}

static void
shell_escape (const char *arg, int from_tty)
{
#if defined(CANT_FORK) || \
      (!defined(HAVE_WORKING_VFORK) && !defined(HAVE_WORKING_FORK))
  /* If ARG is NULL, they want an inferior shell, but `system' just
     reports if the shell is available when passed a NULL arg.  */
  int rc = system (arg ? arg : "");

  if (!arg)
    arg = "inferior shell";

  if (rc == -1)
    fprintf_unfiltered (gdb_stderr, "Cannot execute %s: %s\n", arg,
			safe_strerror (errno));
  else if (rc)
    fprintf_unfiltered (gdb_stderr, "%s exited with status %d\n", arg, rc);
#ifdef GLOBAL_CURDIR
  /* Make sure to return to the directory GDB thinks it is, in case
     the shell command we just ran changed it.  */
  chdir (current_directory);
  exit_status_set_internal_vars (rc);
#endif
#else /* Can fork.  */
  int status, pid;

  if ((pid = vfork ()) == 0)
    {
      const char *p, *user_shell = get_shell ();

      close_most_fds ();

      /* Get the name of the shell for arg0.  */
      p = lbasename (user_shell);

      if (!arg)
	execl (user_shell, p, (char *) 0);
      else
	execl (user_shell, p, "-c", arg, (char *) 0);

      fprintf_unfiltered (gdb_stderr, "Cannot execute %s: %s\n", user_shell,
			  safe_strerror (errno));
      _exit (0177);
    }

  if (pid != -1)
    waitpid (pid, &status, 0);
  else
    error (_("Fork failed"));
  exit_status_set_internal_vars (status);
#endif /* Can fork.  */
}

/* Implementation of the "shell" command.  */

static void
shell_command (const char *arg, int from_tty)
{
  shell_escape (arg, from_tty);
}

static void
edit_command (const char *arg, int from_tty)
{
  struct symtab_and_line sal;
  struct symbol *sym;
  const char *editor;
  char *p;
  const char *fn;

  /* Pull in the current default source line if necessary.  */
  if (arg == 0)
    {
      set_default_source_symtab_and_line ();
      sal = get_current_source_symtab_and_line ();
    }

  /* Bare "edit" edits file with present line.  */

  if (arg == 0)
    {
      if (sal.symtab == 0)
	error (_("No default source file yet."));
      sal.line += get_lines_to_list () / 2;
    }
  else
    {
      const char *arg1;

      /* Now should only be one argument -- decode it in SAL.  */
      arg1 = arg;
      event_location_up location = string_to_event_location (&arg1,
							     current_language);
      std::vector<symtab_and_line> sals = decode_line_1 (location.get (),
							 DECODE_LINE_LIST_MODE,
							 NULL, NULL, 0);

      filter_sals (sals);
      if (sals.empty ())
	{
	  /*  C++  */
	  return;
	}
      if (sals.size () > 1)
	{
	  ambiguous_line_spec (sals,
			       _("Specified line is ambiguous:\n"));
	  return;
	}

      sal = sals[0];

      if (*arg1)
        error (_("Junk at end of line specification."));

      /* If line was specified by address, first print exactly which
         line, and which file.  In this case, sal.symtab == 0 means
         address is outside of all known source files, not that user
         failed to give a filename.  */
      if (*arg == '*')
        {
	  struct gdbarch *gdbarch;

          if (sal.symtab == 0)
	    error (_("No source file for address %s."),
		   paddress (get_current_arch (), sal.pc));

	  gdbarch = get_objfile_arch (SYMTAB_OBJFILE (sal.symtab));
          sym = find_pc_function (sal.pc);
          if (sym)
	    printf_filtered ("%s is in %s (%s:%d).\n",
			     paddress (gdbarch, sal.pc),
			     SYMBOL_PRINT_NAME (sym),
			     symtab_to_filename_for_display (sal.symtab),
			     sal.line);
          else
	    printf_filtered ("%s is at %s:%d.\n",
			     paddress (gdbarch, sal.pc),
			     symtab_to_filename_for_display (sal.symtab),
			     sal.line);
        }

      /* If what was given does not imply a symtab, it must be an
         undebuggable symbol which means no source code.  */

      if (sal.symtab == 0)
        error (_("No line number known for %s."), arg);
    }

  if ((editor = getenv ("EDITOR")) == NULL)
    editor = "/bin/ex";

  fn = symtab_to_fullname (sal.symtab);

  /* Quote the file name, in case it has whitespace or other special
     characters.  */
  p = xstrprintf ("%s +%d \"%s\"", editor, sal.line, fn);
  shell_escape (p, from_tty);
  xfree (p);
}

/* Implementation of the "pipe" command.  */

static void
pipe_command (const char *arg, int from_tty)
{
  std::string delim ("|");

  if (arg != nullptr && check_for_argument (&arg, "-d", 2))
    {
      delim = extract_arg (&arg);
      if (delim.empty ())
	error (_("Missing delimiter DELIM after -d"));
    }

  const char *command = arg;
  if (command == nullptr)
    error (_("Missing COMMAND"));

  arg = strstr (arg, delim.c_str ());

  if (arg == nullptr)
    error (_("Missing delimiter before SHELL_COMMAND"));

  std::string gdb_cmd (command, arg - command);

  arg += delim.length (); /* Skip the delimiter.  */

  if (gdb_cmd.empty ())
    {
      repeat_previous ();
      gdb_cmd = skip_spaces (get_saved_command_line ());
      if (gdb_cmd.empty ())
	error (_("No previous command to relaunch"));
    }

  const char *shell_command = skip_spaces (arg);
  if (*shell_command == '\0')
    error (_("Missing SHELL_COMMAND"));

  FILE *to_shell_command = popen (shell_command, "w");

  if (to_shell_command == nullptr)
    error (_("Error launching \"%s\""), shell_command);

  try
    {
      stdio_file pipe_file (to_shell_command);

      execute_command_to_ui_file (&pipe_file, gdb_cmd.c_str (), from_tty);
    }
  catch (...)
    {
      pclose (to_shell_command);
      throw;
    }

  int exit_status = pclose (to_shell_command);

  if (exit_status < 0)
    error (_("shell command \"%s\" failed: %s"), shell_command,
           safe_strerror (errno));
  exit_status_set_internal_vars (exit_status);
}

static void
list_command (const char *arg, int from_tty)
{
  struct symbol *sym;
  const char *arg1;
  int no_end = 1;
  int dummy_end = 0;
  int dummy_beg = 0;
  int linenum_beg = 0;
  const char *p;

  /* Pull in the current default source line if necessary.  */
  if (arg == NULL || ((arg[0] == '+' || arg[0] == '-') && arg[1] == '\0'))
    {
      set_default_source_symtab_and_line ();
      symtab_and_line cursal = get_current_source_symtab_and_line ();

      /* If this is the first "list" since we've set the current
	 source line, center the listing around that line.  */
      if (get_first_line_listed () == 0)
	{
	  int first;

	  first = std::max (cursal.line - get_lines_to_list () / 2, 1);

	  /* A small special case --- if listing backwards, and we
	     should list only one line, list the preceding line,
	     instead of the exact line we've just shown after e.g.,
	     stopping for a breakpoint.  */
	  if (arg != NULL && arg[0] == '-'
	      && get_lines_to_list () == 1 && first > 1)
	    first -= 1;

	  print_source_lines (cursal.symtab, source_lines_range (first), 0);
	}

      /* "l" or "l +" lists next ten lines.  */
      else if (arg == NULL || arg[0] == '+')
	print_source_lines (cursal.symtab,
			    source_lines_range (cursal.line), 0);

      /* "l -" lists previous ten lines, the ones before the ten just
	 listed.  */
      else if (arg[0] == '-')
	{
	  if (get_first_line_listed () == 1)
	    error (_("Already at the start of %s."),
		   symtab_to_filename_for_display (cursal.symtab));
	  source_lines_range range (get_first_line_listed (),
				    source_lines_range::BACKWARD);
	  print_source_lines (cursal.symtab, range, 0);
	}

      return;
    }

  /* Now if there is only one argument, decode it in SAL
     and set NO_END.
     If there are two arguments, decode them in SAL and SAL_END
     and clear NO_END; however, if one of the arguments is blank,
     set DUMMY_BEG or DUMMY_END to record that fact.  */

  if (!have_full_symbols () && !have_partial_symbols ())
    error (_("No symbol table is loaded.  Use the \"file\" command."));

  std::vector<symtab_and_line> sals;
  symtab_and_line sal, sal_end;

  arg1 = arg;
  if (*arg1 == ',')
    dummy_beg = 1;
  else
    {
      event_location_up location = string_to_event_location (&arg1,
							     current_language);
      sals = decode_line_1 (location.get (), DECODE_LINE_LIST_MODE,
			    NULL, NULL, 0);
      filter_sals (sals);
      if (sals.empty ())
	{
	  /*  C++  */
	  return;
	}

      sal = sals[0];
    }

  /* Record whether the BEG arg is all digits.  */

  for (p = arg; p != arg1 && *p >= '0' && *p <= '9'; p++);
  linenum_beg = (p == arg1);

  /* Save the range of the first argument, in case we need to let the
     user know it was ambiguous.  */
  const char *beg = arg;
  size_t beg_len = arg1 - beg;

  while (*arg1 == ' ' || *arg1 == '\t')
    arg1++;
  if (*arg1 == ',')
    {
      no_end = 0;
      if (sals.size () > 1)
	{
	  ambiguous_line_spec (sals,
			       _("Specified first line '%.*s' is ambiguous:\n"),
			       (int) beg_len, beg);
	  return;
	}
      arg1++;
      while (*arg1 == ' ' || *arg1 == '\t')
	arg1++;
      if (*arg1 == 0)
	dummy_end = 1;
      else
	{
	  /* Save the last argument, in case we need to let the user
	     know it was ambiguous.  */
	  const char *end_arg = arg1;

	  event_location_up location
	    = string_to_event_location (&arg1, current_language);

	  std::vector<symtab_and_line> sals_end
	    = (dummy_beg
	       ? decode_line_1 (location.get (), DECODE_LINE_LIST_MODE,
				NULL, NULL, 0)
	       : decode_line_1 (location.get (), DECODE_LINE_LIST_MODE,
				NULL, sal.symtab, sal.line));

	  filter_sals (sals_end);
	  if (sals_end.empty ())
	    return;
	  if (sals_end.size () > 1)
	    {
	      ambiguous_line_spec (sals_end,
				   _("Specified last line '%s' is ambiguous:\n"),
				   end_arg);
	      return;
	    }
	  sal_end = sals_end[0];
	}
    }

  if (*arg1)
    error (_("Junk at end of line specification."));

  if (!no_end && !dummy_beg && !dummy_end
      && sal.symtab != sal_end.symtab)
    error (_("Specified first and last lines are in different files."));
  if (dummy_beg && dummy_end)
    error (_("Two empty args do not say what lines to list."));

  /* If line was specified by address,
     first print exactly which line, and which file.

     In this case, sal.symtab == 0 means address is outside of all
     known source files, not that user failed to give a filename.  */
  if (*arg == '*')
    {
      struct gdbarch *gdbarch;

      if (sal.symtab == 0)
	error (_("No source file for address %s."),
	       paddress (get_current_arch (), sal.pc));

      gdbarch = get_objfile_arch (SYMTAB_OBJFILE (sal.symtab));
      sym = find_pc_function (sal.pc);
      if (sym)
	printf_filtered ("%s is in %s (%s:%d).\n",
			 paddress (gdbarch, sal.pc),
			 SYMBOL_PRINT_NAME (sym),
			 symtab_to_filename_for_display (sal.symtab), sal.line);
      else
	printf_filtered ("%s is at %s:%d.\n",
			 paddress (gdbarch, sal.pc),
			 symtab_to_filename_for_display (sal.symtab), sal.line);
    }

  /* If line was not specified by just a line number, and it does not
     imply a symtab, it must be an undebuggable symbol which means no
     source code.  */

  if (!linenum_beg && sal.symtab == 0)
    error (_("No line number known for %s."), arg);

  /* If this command is repeated with RET,
     turn it into the no-arg variant.  */

  if (from_tty)
    set_repeat_arguments ("");

  if (dummy_beg && sal_end.symtab == 0)
    error (_("No default source file yet.  Do \"help list\"."));
  if (dummy_beg)
    {
      source_lines_range range (sal_end.line + 1,
				source_lines_range::BACKWARD);
      print_source_lines (sal_end.symtab, range, 0);
    }
  else if (sal.symtab == 0)
    error (_("No default source file yet.  Do \"help list\"."));
  else if (no_end)
    {
      for (int i = 0; i < sals.size (); i++)
	{
	  sal = sals[i];
	  int first_line = sal.line - get_lines_to_list () / 2;
	  if (first_line < 1)
	    first_line = 1;
	  if (sals.size () > 1)
	    print_sal_location (sal);
	  print_source_lines (sal.symtab, source_lines_range (first_line), 0);
	}
    }
  else if (dummy_end)
    print_source_lines (sal.symtab, source_lines_range (sal.line), 0);
  else
    print_source_lines (sal.symtab,
			source_lines_range (sal.line, (sal_end.line + 1)),
			0);
}

/* Subroutine of disassemble_command to simplify it.
   Perform the disassembly.
   NAME is the name of the function if known, or NULL.
   [LOW,HIGH) are the range of addresses to disassemble.
   BLOCK is the block to disassemble; it needs to be provided
   when non-contiguous blocks are disassembled; otherwise
   it can be NULL.
   MIXED is non-zero to print source with the assembler.  */

static void
print_disassembly (struct gdbarch *gdbarch, const char *name,
		   CORE_ADDR low, CORE_ADDR high,
		   const struct block *block,
		   gdb_disassembly_flags flags)
{
#if defined(TUI)
  if (!tui_is_window_visible (DISASSEM_WIN))
#endif
    {
      printf_filtered ("Dump of assembler code ");
      if (name != NULL)
	printf_filtered ("for function %s:\n", name);
      if (block == nullptr || BLOCK_CONTIGUOUS_P (block))
        {
	  if (name == NULL)
	    printf_filtered ("from %s to %s:\n",
			     paddress (gdbarch, low), paddress (gdbarch, high));

	  /* Dump the specified range.  */
	  gdb_disassembly (gdbarch, current_uiout, flags, -1, low, high);
	}
      else
        {
	  for (int i = 0; i < BLOCK_NRANGES (block); i++)
	    {
	      CORE_ADDR range_low = BLOCK_RANGE_START (block, i);
	      CORE_ADDR range_high = BLOCK_RANGE_END (block, i);
	      printf_filtered (_("Address range %s to %s:\n"),
			       paddress (gdbarch, range_low),
			       paddress (gdbarch, range_high));
	      gdb_disassembly (gdbarch, current_uiout, flags, -1,
			       range_low, range_high);
	    }
	}
      printf_filtered ("End of assembler dump.\n");
    }
#if defined(TUI)
  else
    {
      tui_show_assembly (gdbarch, low);
    }
#endif
}

/* Subroutine of disassemble_command to simplify it.
   Print a disassembly of the current function according to FLAGS.  */

static void
disassemble_current_function (gdb_disassembly_flags flags)
{
  struct frame_info *frame;
  struct gdbarch *gdbarch;
  CORE_ADDR low, high, pc;
  const char *name;
  const struct block *block;

  frame = get_selected_frame (_("No frame selected."));
  gdbarch = get_frame_arch (frame);
  pc = get_frame_address_in_block (frame);
  if (find_pc_partial_function (pc, &name, &low, &high, &block) == 0)
    error (_("No function contains program counter for selected frame."));
#if defined(TUI)
  /* NOTE: cagney/2003-02-13 The `tui_active' was previously
     `tui_version'.  */
  if (tui_active)
    /* FIXME: cagney/2004-02-07: This should be an observer.  */
    low = tui_get_low_disassembly_address (gdbarch, low, pc);
#endif
  low += gdbarch_deprecated_function_start_offset (gdbarch);

  print_disassembly (gdbarch, name, low, high, block, flags);
}

/* Dump a specified section of assembly code.

   Usage:
     disassemble [/mrs]
       - dump the assembly code for the function of the current pc
     disassemble [/mrs] addr
       - dump the assembly code for the function at ADDR
     disassemble [/mrs] low,high
     disassemble [/mrs] low,+length
       - dump the assembly code in the range [LOW,HIGH), or [LOW,LOW+length)

   A /m modifier will include source code with the assembly in a
   "source centric" view.  This view lists only the file of the first insn,
   even if other source files are involved (e.g., inlined functions), and
   the output is in source order, even with optimized code.  This view is
   considered deprecated as it hasn't been useful in practice.

   A /r modifier will include raw instructions in hex with the assembly.

   A /s modifier will include source code with the assembly, like /m, with
   two important differences:
   1) The output is still in pc address order.
   2) File names and contents for all relevant source files are displayed.  */

static void
disassemble_command (const char *arg, int from_tty)
{
  struct gdbarch *gdbarch = get_current_arch ();
  CORE_ADDR low, high;
  const char *name;
  CORE_ADDR pc;
  gdb_disassembly_flags flags;
  const char *p;
  const struct block *block = nullptr;

  p = arg;
  name = NULL;
  flags = 0;

  if (p && *p == '/')
    {
      ++p;

      if (*p == '\0')
	error (_("Missing modifier."));

      while (*p && ! isspace (*p))
	{
	  switch (*p++)
	    {
	    case 'm':
	      flags |= DISASSEMBLY_SOURCE_DEPRECATED;
	      break;
	    case 'r':
	      flags |= DISASSEMBLY_RAW_INSN;
	      break;
	    case 's':
	      flags |= DISASSEMBLY_SOURCE;
	      break;
	    default:
	      error (_("Invalid disassembly modifier."));
	    }
	}

      p = skip_spaces (p);
    }

  if ((flags & (DISASSEMBLY_SOURCE_DEPRECATED | DISASSEMBLY_SOURCE))
      == (DISASSEMBLY_SOURCE_DEPRECATED | DISASSEMBLY_SOURCE))
    error (_("Cannot specify both /m and /s."));

  if (! p || ! *p)
    {
      flags |= DISASSEMBLY_OMIT_FNAME;
      disassemble_current_function (flags);
      return;
    }

  pc = value_as_address (parse_to_comma_and_eval (&p));
  if (p[0] == ',')
    ++p;
  if (p[0] == '\0')
    {
      /* One argument.  */
      if (find_pc_partial_function (pc, &name, &low, &high, &block) == 0)
	error (_("No function contains specified address."));
#if defined(TUI)
      /* NOTE: cagney/2003-02-13 The `tui_active' was previously
	 `tui_version'.  */
      if (tui_active)
	/* FIXME: cagney/2004-02-07: This should be an observer.  */
	low = tui_get_low_disassembly_address (gdbarch, low, pc);
#endif
      low += gdbarch_deprecated_function_start_offset (gdbarch);
      flags |= DISASSEMBLY_OMIT_FNAME;
    }
  else
    {
      /* Two arguments.  */
      int incl_flag = 0;
      low = pc;
      p = skip_spaces (p);
      if (p[0] == '+')
	{
	  ++p;
	  incl_flag = 1;
	}
      high = parse_and_eval_address (p);
      if (incl_flag)
	high += low;
    }

  print_disassembly (gdbarch, name, low, high, block, flags);
}

static void
make_command (const char *arg, int from_tty)
{
  if (arg == 0)
    shell_escape ("make", from_tty);
  else
    {
      std::string cmd = std::string ("make ") + arg;

      shell_escape (cmd.c_str (), from_tty);
    }
}

static void
show_user (const char *args, int from_tty)
{
  struct cmd_list_element *c;
  extern struct cmd_list_element *cmdlist;

  if (args)
    {
      const char *comname = args;

      c = lookup_cmd (&comname, cmdlist, "", 0, 1);
      if (!cli_user_command_p (c))
	error (_("Not a user command."));
      show_user_1 (c, "", args, gdb_stdout);
    }
  else
    {
      for (c = cmdlist; c; c = c->next)
	{
	  if (cli_user_command_p (c) || c->prefixlist != NULL)
	    show_user_1 (c, "", c->name, gdb_stdout);
	}
    }
}

/* Search through names of commands and documentations for a certain
   regular expression.  */

static void
apropos_command (const char *arg, int from_tty)
{
  bool verbose = arg && check_for_argument (&arg, "-v", 2);

  if (verbose)
    arg = skip_spaces (arg);

  if (arg == NULL || *arg == '\0')
    error (_("REGEXP string is empty"));

  compiled_regex pattern (arg, REG_ICASE,
			  _("Error in regular expression"));

  apropos_cmd (gdb_stdout, cmdlist, verbose, pattern, "");
}

/* Subroutine of alias_command to simplify it.
   Return the first N elements of ARGV flattened back to a string
   with a space separating each element.
   ARGV may not be NULL.
   This does not take care of quoting elements in case they contain spaces
   on purpose.  */

static std::string
argv_to_string (char **argv, int n)
{
  int i;
  std::string result;

  gdb_assert (argv != NULL);
  gdb_assert (n >= 0 && n <= countargv (argv));

  for (i = 0; i < n; ++i)
    {
      if (i > 0)
	result += " ";
      result += argv[i];
    }

  return result;
}

/* Subroutine of alias_command to simplify it.
   Return true if COMMAND exists, unambiguously.  Otherwise false.  */

static bool
valid_command_p (const char *command)
{
  struct cmd_list_element *c;

  c = lookup_cmd_1 (& command, cmdlist, NULL, 1);

  if (c == NULL || c == (struct cmd_list_element *) -1)
    return false;

  /* This is the slightly tricky part.
     lookup_cmd_1 will return a pointer to the last part of COMMAND
     to match, leaving COMMAND pointing at the remainder.  */
  while (*command == ' ' || *command == '\t')
    ++command;
  return *command == '\0';
}

/* Called when "alias" was incorrectly used.  */

static void
alias_usage_error (void)
{
  error (_("Usage: alias [-a] [--] ALIAS = COMMAND"));
}

/* Make an alias of an existing command.  */

static void
alias_command (const char *args, int from_tty)
{
  int i, alias_argc, command_argc;
  int abbrev_flag = 0;
  const char *equals;
  const char *alias, *command;

  if (args == NULL || strchr (args, '=') == NULL)
    alias_usage_error ();

  equals = strchr (args, '=');
  std::string args2 (args, equals - args);

  gdb_argv built_alias_argv (args2.c_str ());
  gdb_argv command_argv (equals + 1);

  char **alias_argv = built_alias_argv.get ();
  while (alias_argv[0] != NULL)
    {
      if (strcmp (alias_argv[0], "-a") == 0)
	{
	  ++alias_argv;
	  abbrev_flag = 1;
	}
      else if (strcmp (alias_argv[0], "--") == 0)
	{
	  ++alias_argv;
	  break;
	}
      else
	break;
    }

  if (alias_argv[0] == NULL || command_argv[0] == NULL
      || *alias_argv[0] == '\0' || *command_argv[0] == '\0')
    alias_usage_error ();

  for (i = 0; alias_argv[i] != NULL; ++i)
    {
      if (! valid_user_defined_cmd_name_p (alias_argv[i]))
	{
	  if (i == 0)
	    error (_("Invalid command name: %s"), alias_argv[i]);
	  else
	    error (_("Invalid command element name: %s"), alias_argv[i]);
	}
    }

  alias_argc = countargv (alias_argv);
  command_argc = command_argv.count ();

  /* COMMAND must exist.
     Reconstruct the command to remove any extraneous spaces,
     for better error messages.  */
  std::string command_string (argv_to_string (command_argv.get (),
					      command_argc));
  command = command_string.c_str ();
  if (! valid_command_p (command))
    error (_("Invalid command to alias to: %s"), command);

  /* ALIAS must not exist.  */
  std::string alias_string (argv_to_string (alias_argv, alias_argc));
  alias = alias_string.c_str ();
  if (valid_command_p (alias))
    error (_("Alias already exists: %s"), alias);

  /* If ALIAS is one word, it is an alias for the entire COMMAND.
     Example: alias spe = set print elements

     Otherwise ALIAS and COMMAND must have the same number of words,
     and every word except the last must match; and the last word of
     ALIAS is made an alias of the last word of COMMAND.
     Example: alias set print elms = set pr elem
     Note that unambiguous abbreviations are allowed.  */

  if (alias_argc == 1)
    {
      /* add_cmd requires *we* allocate space for name, hence the xstrdup.  */
      add_com_alias (xstrdup (alias_argv[0]), command, class_alias,
		     abbrev_flag);
    }
  else
    {
      const char *alias_prefix, *command_prefix;
      struct cmd_list_element *c_alias, *c_command;

      if (alias_argc != command_argc)
	error (_("Mismatched command length between ALIAS and COMMAND."));

      /* Create copies of ALIAS and COMMAND without the last word,
	 and use that to verify the leading elements match.  */
      std::string alias_prefix_string (argv_to_string (alias_argv,
						       alias_argc - 1));
      std::string command_prefix_string (argv_to_string (alias_argv,
							 command_argc - 1));
      alias_prefix = alias_prefix_string.c_str ();
      command_prefix = command_prefix_string.c_str ();

      c_command = lookup_cmd_1 (& command_prefix, cmdlist, NULL, 1);
      /* We've already tried to look up COMMAND.  */
      gdb_assert (c_command != NULL
		  && c_command != (struct cmd_list_element *) -1);
      gdb_assert (c_command->prefixlist != NULL);
      c_alias = lookup_cmd_1 (& alias_prefix, cmdlist, NULL, 1);
      if (c_alias != c_command)
	error (_("ALIAS and COMMAND prefixes do not match."));

      /* add_cmd requires *we* allocate space for name, hence the xstrdup.  */
      add_alias_cmd (xstrdup (alias_argv[alias_argc - 1]),
		     command_argv[command_argc - 1],
		     class_alias, abbrev_flag, c_command->prefixlist);
    }
}

/* Print the file / line number / symbol name of the location
   specified by SAL.  */

static void
print_sal_location (const symtab_and_line &sal)
{
  scoped_restore_current_program_space restore_pspace;
  set_current_program_space (sal.pspace);

  const char *sym_name = NULL;
  if (sal.symbol != NULL)
    sym_name = SYMBOL_PRINT_NAME (sal.symbol);
  printf_filtered (_("file: \"%s\", line number: %d, symbol: \"%s\"\n"),
		   symtab_to_filename_for_display (sal.symtab),
		   sal.line, sym_name != NULL ? sym_name : "???");
}

/* Print a list of files and line numbers which a user may choose from
   in order to list a function which was specified ambiguously (as
   with `list classname::overloadedfuncname', for example).  The SALS
   array provides the filenames and line numbers.  FORMAT is a
   printf-style format string used to tell the user what was
   ambiguous.  */

static void
ambiguous_line_spec (gdb::array_view<const symtab_and_line> sals,
		     const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  vprintf_filtered (format, ap);
  va_end (ap);

  for (const auto &sal : sals)
    print_sal_location (sal);
}

/* Comparison function for filter_sals.  Returns a qsort-style
   result.  */

static int
cmp_symtabs (const symtab_and_line &sala, const symtab_and_line &salb)
{
  const char *dira = SYMTAB_DIRNAME (sala.symtab);
  const char *dirb = SYMTAB_DIRNAME (salb.symtab);
  int r;

  if (dira == NULL)
    {
      if (dirb != NULL)
	return -1;
    }
  else if (dirb == NULL)
    {
      if (dira != NULL)
	return 1;
    }
  else
    {
      r = filename_cmp (dira, dirb);
      if (r)
	return r;
    }

  r = filename_cmp (sala.symtab->filename, salb.symtab->filename);
  if (r)
    return r;

  if (sala.line < salb.line)
    return -1;
  return sala.line == salb.line ? 0 : 1;
}

/* Remove any SALs that do not match the current program space, or
   which appear to be "file:line" duplicates.  */

static void
filter_sals (std::vector<symtab_and_line> &sals)
{
  /* Remove SALs that do not match.  */
  auto from = std::remove_if (sals.begin (), sals.end (),
			      [&] (const symtab_and_line &sal)
    { return (sal.pspace != current_program_space || sal.symtab == NULL); });

  /* Remove dups.  */
  std::sort (sals.begin (), from,
	     [] (const symtab_and_line &sala, const symtab_and_line &salb)
   { return cmp_symtabs (sala, salb) < 0; });

  from = std::unique (sals.begin (), from,
		      [&] (const symtab_and_line &sala,
			   const symtab_and_line &salb)
    { return cmp_symtabs (sala, salb) == 0; });

  sals.erase (from, sals.end ());
}

static void
set_debug (const char *arg, int from_tty)
{
  printf_unfiltered (_("\"set debug\" must be followed by "
		       "the name of a debug subcommand.\n"));
  help_list (setdebuglist, "set debug ", all_commands, gdb_stdout);
}

static void
show_debug (const char *args, int from_tty)
{
  cmd_show_list (showdebuglist, from_tty, "");
}

void
init_cmd_lists (void)
{
  max_user_call_depth = 1024;
}

static void
show_info_verbose (struct ui_file *file, int from_tty,
		   struct cmd_list_element *c,
		   const char *value)
{
  if (info_verbose)
    fprintf_filtered (file,
		      _("Verbose printing of informational messages is %s.\n"),
		      value);
  else
    fprintf_filtered (file, _("Verbosity is %s.\n"), value);
}

static void
show_history_expansion_p (struct ui_file *file, int from_tty,
			  struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("History expansion on command input is %s.\n"),
		    value);
}

static void
show_remote_debug (struct ui_file *file, int from_tty,
		   struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Debugging of remote protocol is %s.\n"),
		    value);
}

static void
show_remote_timeout (struct ui_file *file, int from_tty,
		     struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file,
		    _("Timeout limit to wait for target to respond is %s.\n"),
		    value);
}

static void
show_max_user_call_depth (struct ui_file *file, int from_tty,
			  struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file,
		    _("The max call depth for user-defined commands is %s.\n"),
		    value);
}

void
_initialize_cli_cmds (void)
{
  struct cmd_list_element *c;

  /* Define the classes of commands.
     They will appear in the help list in alphabetical order.  */

  add_cmd ("internals", class_maintenance, _("\
Maintenance commands.\n\
Some gdb commands are provided just for use by gdb maintainers.\n\
These commands are subject to frequent change, and may not be as\n\
well documented as user commands."),
	   &cmdlist);
  add_cmd ("obscure", class_obscure, _("Obscure features."), &cmdlist);
  add_cmd ("aliases", class_alias,
	   _("Aliases of other commands."), &cmdlist);
  add_cmd ("user-defined", class_user, _("\
User-defined commands.\n\
The commands in this class are those defined by the user.\n\
Use the \"define\" command to define a command."), &cmdlist);
  add_cmd ("support", class_support, _("Support facilities."), &cmdlist);
  if (!dbx_commands)
    add_cmd ("status", class_info, _("Status inquiries."), &cmdlist);
  add_cmd ("files", class_files, _("Specifying and examining files."),
	   &cmdlist);
  add_cmd ("breakpoints", class_breakpoint,
	   _("Making program stop at certain points."), &cmdlist);
  add_cmd ("data", class_vars, _("Examining data."), &cmdlist);
  add_cmd ("stack", class_stack, _("\
Examining the stack.\n\
The stack is made up of stack frames.  Gdb assigns numbers to stack frames\n\
counting from zero for the innermost (currently executing) frame.\n\n\
At any time gdb identifies one frame as the \"selected\" frame.\n\
Variable lookups are done with respect to the selected frame.\n\
When the program being debugged stops, gdb selects the innermost frame.\n\
The commands below can be used to select other frames by number or address."),
	   &cmdlist);
  add_cmd ("running", class_run, _("Running the program."), &cmdlist);

  /* Define general commands.  */

  add_com ("pwd", class_files, pwd_command, _("\
Print working directory.  This is used for your program as well."));

  c = add_cmd ("cd", class_files, cd_command, _("\
Set working directory to DIR for debugger.\n\
The debugger's current working directory specifies where scripts and other\n\
files that can be loaded by GDB are located.\n\
In order to change the inferior's current working directory, the recommended\n\
way is to use the \"set cwd\" command."), &cmdlist);
  set_cmd_completer (c, filename_completer);

  add_com ("echo", class_support, echo_command, _("\
Print a constant string.  Give string as argument.\n\
C escape sequences may be used in the argument.\n\
No newline is added at the end of the argument;\n\
use \"\\n\" if you want a newline to be printed.\n\
Since leading and trailing whitespace are ignored in command arguments,\n\
if you want to print some you must use \"\\\" before leading whitespace\n\
to be printed or after trailing whitespace."));

  add_setshow_enum_cmd ("script-extension", class_support,
			script_ext_enums, &script_ext_mode, _("\
Set mode for script filename extension recognition."), _("\
Show mode for script filename extension recognition."), _("\
off  == no filename extension recognition (all sourced files are GDB scripts)\n\
soft == evaluate script according to filename extension, fallback to GDB script"
  "\n\
strict == evaluate script according to filename extension, error if not supported"
  ),
			NULL,
			show_script_ext_mode,
			&setlist, &showlist);

  add_com ("quit", class_support, quit_command, _("\
Exit gdb.\n\
Usage: quit [EXPR]\n\
The optional expression EXPR, if present, is evaluated and the result\n\
used as GDB's exit code.  The default is zero."));
  c = add_com ("help", class_support, help_command,
	       _("Print list of commands."));
  set_cmd_completer (c, command_completer);
  add_com_alias ("q", "quit", class_support, 1);
  add_com_alias ("h", "help", class_support, 1);

  add_setshow_boolean_cmd ("verbose", class_support, &info_verbose, _("\
Set verbosity."), _("\
Show verbosity."), NULL,
			   set_verbose,
			   show_info_verbose,
			   &setlist, &showlist);

  add_prefix_cmd ("history", class_support, set_history,
		  _("Generic command for setting command history parameters."),
		  &sethistlist, "set history ", 0, &setlist);
  add_prefix_cmd ("history", class_support, show_history,
		  _("Generic command for showing command history parameters."),
		  &showhistlist, "show history ", 0, &showlist);

  add_setshow_boolean_cmd ("expansion", no_class, &history_expansion_p, _("\
Set history expansion on command input."), _("\
Show history expansion on command input."), _("\
Without an argument, history expansion is enabled."),
			   NULL,
			   show_history_expansion_p,
			   &sethistlist, &showhistlist);

  add_prefix_cmd ("info", class_info, info_command, _("\
Generic command for showing things about the program being debugged."),
		  &infolist, "info ", 0, &cmdlist);
  add_com_alias ("i", "info", class_info, 1);
  add_com_alias ("inf", "info", class_info, 1);

  add_com ("complete", class_obscure, complete_command,
	   _("List the completions for the rest of the line as a command."));

  add_prefix_cmd ("show", class_info, show_command, _("\
Generic command for showing things about the debugger."),
		  &showlist, "show ", 0, &cmdlist);
  /* Another way to get at the same thing.  */
  add_info ("set", show_command, _("Show all GDB settings."));

  add_cmd ("commands", no_set_class, show_commands, _("\
Show the history of commands you typed.\n\
You can supply a command number to start with, or a `+' to start after\n\
the previous command number shown."),
	   &showlist);

  add_cmd ("version", no_set_class, show_version,
	   _("Show what version of GDB this is."), &showlist);

  add_cmd ("configuration", no_set_class, show_configuration,
	   _("Show how GDB was configured at build time."), &showlist);

  add_setshow_zinteger_cmd ("remote", no_class, &remote_debug, _("\
Set debugging of remote protocol."), _("\
Show debugging of remote protocol."), _("\
When enabled, each packet sent or received with the remote target\n\
is displayed."),
			    NULL,
			    show_remote_debug,
			    &setdebuglist, &showdebuglist);

  add_setshow_zuinteger_unlimited_cmd ("remotetimeout", no_class,
				       &remote_timeout, _("\
Set timeout limit to wait for target to respond."), _("\
Show timeout limit to wait for target to respond."), _("\
This value is used to set the time limit for gdb to wait for a response\n\
from the target."),
				       NULL,
				       show_remote_timeout,
				       &setlist, &showlist);

  add_prefix_cmd ("debug", no_class, set_debug,
		  _("Generic command for setting gdb debugging flags"),
		  &setdebuglist, "set debug ", 0, &setlist);

  add_prefix_cmd ("debug", no_class, show_debug,
		  _("Generic command for showing gdb debugging flags"),
		  &showdebuglist, "show debug ", 0, &showlist);

  c = add_com ("shell", class_support, shell_command, _("\
Execute the rest of the line as a shell command.\n\
With no arguments, run an inferior shell."));
  set_cmd_completer (c, filename_completer);

  add_com_alias ("!", "shell", class_support, 0);

  c = add_com ("edit", class_files, edit_command, _("\
Edit specified file or function.\n\
With no argument, edits file containing most recent line listed.\n\
Editing targets can be specified in these ways:\n\
  FILE:LINENUM, to edit at that line in that file,\n\
  FUNCTION, to edit at the beginning of that function,\n\
  FILE:FUNCTION, to distinguish among like-named static functions.\n\
  *ADDRESS, to edit at the line containing that address.\n\
Uses EDITOR environment variable contents as editor (or ex as default)."));

  c->completer = location_completer;

  c = add_com ("pipe", class_support, pipe_command, _("\
Send the output of a gdb command to a shell command.\n\
Usage: | [COMMAND] | SHELL_COMMAND\n\
Usage: | -d DELIM COMMAND DELIM SHELL_COMMAND\n\
Usage: pipe [COMMAND] | SHELL_COMMAND\n\
Usage: pipe -d DELIM COMMAND DELIM SHELL_COMMAND\n\
\n\
Executes COMMAND and sends its output to SHELL_COMMAND.\n\
\n\
The -d option indicates to use the string DELIM to separate COMMAND\n\
from SHELL_COMMAND, in alternative to |.  This is useful in\n\
case COMMAND contains a | character.\n\
\n\
With no COMMAND, repeat the last executed command\n\
and send its output to SHELL_COMMAND."));
  add_com_alias ("|", "pipe", class_support, 0);

  add_com ("list", class_files, list_command, _("\
List specified function or line.\n\
With no argument, lists ten more lines after or around previous listing.\n\
\"list -\" lists the ten lines before a previous ten-line listing.\n\
One argument specifies a line, and ten lines are listed around that line.\n\
Two arguments with comma between specify starting and ending lines to list.\n\
Lines can be specified in these ways:\n\
  LINENUM, to list around that line in current file,\n\
  FILE:LINENUM, to list around that line in that file,\n\
  FUNCTION, to list around beginning of that function,\n\
  FILE:FUNCTION, to distinguish among like-named static functions.\n\
  *ADDRESS, to list around the line containing that address.\n\
With two args, if one is empty, it stands for ten lines away from\n\
the other arg.\n\
\n\
By default, when a single location is given, display ten lines.\n\
This can be changed using \"set listsize\", and the current value\n\
can be shown using \"show listsize\"."));

  add_com_alias ("l", "list", class_files, 1);

  if (dbx_commands)
    add_com_alias ("file", "list", class_files, 1);

  c = add_com ("disassemble", class_vars, disassemble_command, _("\
Disassemble a specified section of memory.\n\
Default is the function surrounding the pc of the selected frame.\n\
\n\
With a /m modifier, source lines are included (if available).\n\
This view is \"source centric\": the output is in source line order,\n\
regardless of any optimization that is present.  Only the main source file\n\
is displayed, not those of, e.g., any inlined functions.\n\
This modifier hasn't proved useful in practice and is deprecated\n\
in favor of /s.\n\
\n\
With a /s modifier, source lines are included (if available).\n\
This differs from /m in two important respects:\n\
- the output is still in pc address order, and\n\
- file names and contents for all relevant source files are displayed.\n\
\n\
With a /r modifier, raw instructions in hex are included.\n\
\n\
With a single argument, the function surrounding that address is dumped.\n\
Two arguments (separated by a comma) are taken as a range of memory to dump,\n\
  in the form of \"start,end\", or \"start,+length\".\n\
\n\
Note that the address is interpreted as an expression, not as a location\n\
like in the \"break\" command.\n\
So, for example, if you want to disassemble function bar in file foo.c\n\
you must type \"disassemble 'foo.c'::bar\" and not \"disassemble foo.c:bar\"."));
  set_cmd_completer (c, location_completer);

  c = add_com ("make", class_support, make_command, _("\
Run the ``make'' program using the rest of the line as arguments."));
  set_cmd_completer (c, filename_completer);
  add_cmd ("user", no_class, show_user, _("\
Show definitions of non-python/scheme user defined commands.\n\
Argument is the name of the user defined command.\n\
With no argument, show definitions of all user defined commands."), &showlist);
  add_com ("apropos", class_support, apropos_command, _("\
Search for commands matching a REGEXP\n\
Usage: apropos [-v] REGEXP\n\
Flag -v indicates to produce a verbose output, showing full documentation\n\
of the matching commands."));

  add_setshow_uinteger_cmd ("max-user-call-depth", no_class,
			   &max_user_call_depth, _("\
Set the max call depth for non-python/scheme user-defined commands."), _("\
Show the max call depth for non-python/scheme user-defined commands."), NULL,
			    NULL,
			    show_max_user_call_depth,
			    &setlist, &showlist);

  add_setshow_boolean_cmd ("trace-commands", no_class, &trace_commands, _("\
Set tracing of GDB CLI commands."), _("\
Show state of GDB CLI command tracing."), _("\
When 'on', each command is displayed as it is executed."),
			   NULL,
			   NULL,
			   &setlist, &showlist);

  c = add_com ("alias", class_support, alias_command, _("\
Define a new command that is an alias of an existing command.\n\
Usage: alias [-a] [--] ALIAS = COMMAND\n\
ALIAS is the name of the alias command to create.\n\
COMMAND is the command being aliased to.\n\
If \"-a\" is specified, the command is an abbreviation,\n\
and will not appear in help command list output.\n\
\n\
Examples:\n\
Make \"spe\" an alias of \"set print elements\":\n\
  alias spe = set print elements\n\
Make \"elms\" an alias of \"elements\" in the \"set print\" command:\n\
  alias -a set print elms = set print elements"));
}

void
init_cli_cmds (void)
{
  struct cmd_list_element *c;
  char *source_help_text;

  source_help_text = xstrprintf (_("\
Read commands from a file named FILE.\n\
\n\
Usage: source [-s] [-v] FILE\n\
-s: search for the script in the source search path,\n\
    even if FILE contains directories.\n\
-v: each command in FILE is echoed as it is executed.\n\
\n\
Note that the file \"%s\" is read automatically in this way\n\
when GDB is started."), gdbinit);
  c = add_cmd ("source", class_support, source_command,
	       source_help_text, &cmdlist);
  set_cmd_completer (c, filename_completer);
}
