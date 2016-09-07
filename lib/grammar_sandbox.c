/*
 * Testing shim and API examples for the new CLI backend.
 *
 * This unit defines a number of commands in the old engine that can
 * be used to test and interact with the new engine.
 *
 * This shim should be removed upon integration. It is currently hooked in
 * vtysh/vtysh.c. It has no header, vtysh.c merely includes this entire unit
 * since it clutters up the makefiles less and this is only a temporary shim.
 *
 * --
 * Copyright (C) 2016 Cumulus Networks, Inc.
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "command.h"
#include "graph.h"
#include "command_parse.h"
#include "command_match.h"

#define GRAMMAR_STR "CLI grammar sandbox\n"

void
grammar_sandbox_init (void);
void
pretty_print_graph (struct graph_node *, int);

/*
 * Start node for testing command graph.
 *
 * Each cmd_node will have one of these that replaces the `cmdvector` member.
 * The examples below show how to install a command to the graph, calculate
 * completions for a given input line, and match input against the graph.
 */
struct graph *nodegraph;

/**
 * Reference use of parsing / command installation API
 */
DEFUN (grammar_test,
       grammar_test_cmd,
       "grammar parse .COMMAND",
       GRAMMAR_STR
       "command to pass to new parser\n")
{
  char *command = argv_concat(argv, argc, 0);

  // initialize a pretend cmd_element
  struct cmd_element *cmd = XCALLOC (MTYPE_CMD_TOKENS, sizeof (struct cmd_element));
  cmd->string = XSTRDUP (MTYPE_TMP, command);
  cmd->doc = NULL;
  cmd->func = NULL;
  cmd->tokens = vector_init (VECTOR_MIN_SIZE);

  // parse the command and install it into the command graph
  command_parse_format (nodegraph, cmd);

  // free resources
  free (command);

  return CMD_SUCCESS;
}


/**
 * Reference use of completions API
 */
DEFUN (grammar_test_complete,
       grammar_test_complete_cmd,
       "grammar complete .COMMAND",
       GRAMMAR_STR
       "attempt to complete input on DFA\n"
       "command to complete")
{
  char *cmdstr = argv_concat (argv, argc, 0);
  vector command = cmd_make_strvec (cmdstr);

  struct list *completions = list_new ();
  enum matcher_rv result =
     command_complete (nodegraph, command, &completions);

  // print completions or relevant error message
  if (!MATCHER_ERROR(result))
    {
      struct listnode *ln;
      struct cmd_token_t *tkn;
      for (ALL_LIST_ELEMENTS_RO(completions,ln,tkn))
        zlog_info (tkn->text);
    }
  else
    zlog_info ("%% No match for \"%s\"", cmdstr);

  // free resources
  cmd_free_strvec (command);
  list_delete (completions);
  free (cmdstr);

  return CMD_SUCCESS;
}

/**
 * Reference use of matching API
 */
DEFUN (grammar_test_match,
       grammar_test_match_cmd,
       "grammar match .COMMAND",
       GRAMMAR_STR
       "attempt to match input on DFA\n"
       "command to match")
{
  char *cmdstr = argv_concat(argv, argc, 0);
  if (cmdstr[0] == '#')
    return CMD_SUCCESS;

  vector command = cmd_make_strvec (cmdstr);

  struct list *argvv = NULL;
  struct cmd_element *element = NULL;
  enum matcher_rv result = command_match (nodegraph, command, &argvv, &element);

  // print completions or relevant error message
  if (element)
    {
      zlog_info ("Matched: %s", element->string);
      struct listnode *ln;
      struct cmd_token_t *token;
      for (ALL_LIST_ELEMENTS_RO(argvv,ln,token))
        zlog_info ("%s -- %s", token->text, token->arg);

      zlog_info ("func: %p", element->func);

      list_delete (argvv);
    }
  else {
     assert(MATCHER_ERROR(result));
     switch (result) {
       case MATCHER_NO_MATCH:
          zlog_info ("%% Unknown command");
          break;
       case MATCHER_INCOMPLETE:
          zlog_info ("%% Incomplete command");
          break;
       case MATCHER_AMBIGUOUS:
          zlog_info ("%% Ambiguous command");
          break;
       default:
          zlog_info ("%% Unknown error");
          break;
     }
  }

  // free resources
  cmd_free_strvec(command);
  free(cmdstr);

  return CMD_SUCCESS;
}

/**
 * Testing shim to test docstrings
 */
DEFUN (grammar_test_doc,
       grammar_test_doc_cmd,
       "grammar test docstring",
       GRAMMAR_STR
       "Test function for docstring\n"
       "Command end\n")
{
  // create cmd_element with docstring
  struct cmd_element *cmd = XCALLOC (MTYPE_CMD_TOKENS, sizeof (struct cmd_element));
  cmd->string = "test docstring <example|selector follow> (1-255) end VARIABLE [OPTION|set lol] . VARARG";
  cmd->doc = "Test stuff\n"
             "docstring thing\n"
             "first example\n"
             "second example\n"
             "follow\n"
             "random range\n"
             "end thingy\n"
             "variable\n"
             "optional variable\n"
             "optional set\n"
             "optional lol\n"
             "vararg!\n";
  cmd->func = NULL;
  cmd->tokens = vector_init (VECTOR_MIN_SIZE);

  // parse element
  command_parse_format (nodegraph, cmd);

  return CMD_SUCCESS;
}

/**
 * Debugging command to print command graph
 */
DEFUN (grammar_test_show,
       grammar_test_show_cmd,
       "grammar show graph",
       GRAMMAR_STR
       "print current accumulated DFA\n")
{
  if (!nodegraph)
    zlog_info("nodegraph uninitialized");
  else
    pretty_print_graph (vector_slot (nodegraph->nodes, 0), 0);
  return CMD_SUCCESS;
}

/* this is called in vtysh.c to set up the testing shim */
void grammar_sandbox_init() {
  zlog_info ("Initializing grammar testing shim");

  // initialize graph, add start noe
  nodegraph = graph_new ();
  struct cmd_token_t *token = new_cmd_token (START_TKN, NULL, NULL);
  graph_new_node (nodegraph, token, (void (*)(void *)) &del_cmd_token);

  // install all enable elements
  install_element (ENABLE_NODE, &grammar_test_cmd);
  install_element (ENABLE_NODE, &grammar_test_show_cmd);
  install_element (ENABLE_NODE, &grammar_test_match_cmd);
  install_element (ENABLE_NODE, &grammar_test_complete_cmd);
  install_element (ENABLE_NODE, &grammar_test_doc_cmd);
}

/**
 * Pretty-prints a graph, assuming it is a tree.
 *
 * @param start the node to take as the root
 * @param level indent level for recursive calls, always pass 0
 */
void
pretty_print_graph (struct graph_node *start, int level)
{
  // print this node
  struct cmd_token_t *tok = start->data;
  fprintf (stdout, "%s[%d] ", tok->text, tok->type);

  int numto = vector_active (start->to);
  if (numto)
    {
      if (numto > 1)
        fprintf (stdout, "\n");
      for (unsigned int i = 0; i < vector_active (start->to); i++)
        {
          struct graph_node *adj = vector_slot (start->to, i);
          // if we're listing multiple children, indent!
          if (numto > 1)
            for (int j = 0; j < level+1; j++)
              fprintf (stdout, "    ");
          // if this node is a vararg, just print *
          if (adj == start)
            fprintf (stdout, "*");
          else
            pretty_print_graph (adj, numto > 1 ? level+1 : level);
        }
    }
  else
    fprintf(stdout, "\n");
}

/** stuff that should go in command.c + command.h */
struct cmd_token_t *
new_cmd_token (enum cmd_token_type_t type, char *text, char *desc)
{
  struct cmd_token_t *token = XMALLOC (MTYPE_CMD_TOKENS, sizeof (struct cmd_token_t));
  token->type = type;
  token->text = text;
  token->desc = desc;
  token->arg  = NULL;

  return token;
}

void
del_cmd_token (struct cmd_token_t *token)
{
  XFREE (MTYPE_CMD_TOKENS, token->text);
  XFREE (MTYPE_CMD_TOKENS, token->desc);
  XFREE (MTYPE_CMD_TOKENS, token->arg);
  XFREE (MTYPE_CMD_TOKENS, token);
}

struct cmd_token_t *
copy_cmd_token (struct cmd_token_t *token)
{
  struct cmd_token_t *copy = new_cmd_token (token->type, NULL, NULL);
  copy->text = token->text ? XSTRDUP (MTYPE_CMD_TOKENS, token->text) : NULL;
  copy->desc = token->desc ? XSTRDUP (MTYPE_CMD_TOKENS, token->desc) : NULL;
  copy->arg  = token->arg  ? XSTRDUP (MTYPE_CMD_TOKENS, token->arg) : NULL;

  return copy;
}
